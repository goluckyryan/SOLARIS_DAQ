#ifndef HISTOGRAM_1D_H
#define HISTOGRAM_1D_H

#include "qcustomplot.h"
#include "macro.h"
#include <QInputDialog>
#include <QHBoxLayout>
#include <QLabel>

#define MaxNHist 10

//^==============================================
//^==============================================
class Histogram1D : public QCustomPlot{
  Q_OBJECT
public:
  Histogram1D(QString title, QString xLabel, int xbin, double xmin, double xmax, QWidget * parent = nullptr) : QCustomPlot(parent){
    // DebugPrint("%s", "Histogram1D");
    isLogY = false;

    for( int i = 0; i < MaxNHist; i++ ) showHist[i] = true;

    for( int i = 0; i < 3; i ++) txt[i] = nullptr;
    nData = 1;
    Rebin(xbin, xmin, xmax);

    xAxis->setLabel(xLabel);

    legend->setVisible(true);
    QPen borderPen = legend->borderPen();
    borderPen.setWidth(0);
    borderPen.setColor(Qt::transparent);
    legend->setBorderPen(borderPen);
    legend->setFont(QFont("Helvetica", 9));

    addGraph();
    graph(0)->setName(title);
    graph(0)->setLineStyle(QCPGraph::lsStepCenter);
    graph(0)->setPen(QPen(Qt::blue));
    graph(0)->setBrush(QBrush(QColor(0, 0, 255, 20)));

    xAxis2->setVisible(true);
    xAxis2->setTickLabels(false);
    yAxis2->setVisible(true);
    yAxis2->setTickLabels(false);
    // make left and bottom axes always transfer their ranges to right and top axes:
    connect(xAxis, SIGNAL(rangeChanged(QCPRange)), xAxis2, SLOT(setRange(QCPRange)));
    connect(yAxis, SIGNAL(rangeChanged(QCPRange)), yAxis2, SLOT(setRange(QCPRange)));

    graph(0)->setData(xList, yList[0]);

    //setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectPlottables);
    //setInteractions( QCP::iRangeDrag | QCP::iRangeZoom );

    //setSelectionRectMode(QCP::SelectionRectMode::srmZoom);

    rescaleAxes();
    yAxis->setRangeLower(0);
    yAxis->setRangeUpper(10);

    for( int i = 0; i < 3; i ++){
      txt[i] = new QCPItemText(this);
      txt[i]->setPositionAlignment(Qt::AlignLeft);
      txt[i]->position->setType(QCPItemPosition::ptAxisRectRatio);
      txt[i]->position->setCoords(0.1, 0.1 + 0.1*i);
      txt[i]->setFont(QFont("Helvetica", 9));
    }
    txt[0]->setText("Under Flow : 0");
    txt[1]->setText(TotalEntryLabel());
    txt[2]->setText("Over Flow : 0");
    usingMenu = false;

    /// gate bands / edge lines / rate labels follow the axes on every redraw, incl. zoom+pan
    connect(this, &QCustomPlot::beforeReplot, this, [this](){ UpdateGateGeometry(); });


    connect(this, &QCustomPlot::mouseMove, this, [=](QMouseEvent *event){
      double x = this->xAxis->pixelToCoord(event->pos().x());
      int bin = qFloor((x - xMin)/dX);
      /// The plot is zoomable and draggable, so the cursor reaches coordinates outside [xMin,xMax)
      /// and this used to subscript yList with the resulting out-of-range bin.
      if( bin < 0 || bin >= yList[0].count() ) return;

      QString coordinates = QString("Bin: %1, Value: %2").arg(bin).arg(yList[0][bin]);
      QToolTip::showText(event->globalPosition().toPoint(), coordinates, this);
    });

    connect(this, &QCustomPlot::mousePress, this, [=](QMouseEvent * event){
      if (event->button() == Qt::LeftButton && !usingMenu){
        setSelectionRectMode(QCP::SelectionRectMode::srmZoom);
      }
      if (event->button() == Qt::RightButton) {
        usingMenu = true;
        setSelectionRectMode(QCP::SelectionRectMode::srmNone);

        QMenu menu(this);

        QAction * a1 = menu.addAction("UnZoom");
        QAction * a5 = menu.addAction("Set/UnSet Log-y");
        QAction * a6 = nullptr;
        if( nData > 1 )  a6 = menu.addAction("Toggle lines display");
        QAction * a2 = menu.addAction("Clear hist.");
        QAction * a3 = menu.addAction("Toggle Stat.");
        QAction * a4 = menu.addAction("Rebin (clear histogram)");

        //^---- gates : one band each, edges dragged with the left button
        QAction * gAdd = ( gates.size() < MaxGates ) ? menu.addAction("Add Gate") : nullptr;
        QAction * gRen = nullptr, * gClr = nullptr;
        QList<QAction *> gDelActs;
        if( !gates.isEmpty() ){
          menu.addSeparator();
          gRen = menu.addAction("Edit Gates...");
          gClr = menu.addAction("Clear Gates");
          for( int k = 0; k < gates.size(); k++ ){
            QAction * da = menu.addAction("Delete " + gates[k].name + " [" + gateColorName(gates[k].id) + "]");
            /// The index rides the action (same lesson as the 2D cut menu): recovering it by
            /// parsing the label would break the moment a gate is renamed.
            da->setData(k);
            gDelActs.append(da);
          }
        }

        //^---- (rates show [x.x Hz]; the trailing averaging window is GLOBAL -- the "Time average"
        //^     control next to the Rate Monitor button. Deliberately not a menu here.)
        //TODO fitGuass

        QAction *selectedAction = menu.exec(event->globalPosition().toPoint());
        //*========================================== UnZoom
        if( selectedAction == a1 ){
          xAxis->setRangeLower(xMin);
          xAxis->setRangeUpper(xMax);
          yAxis->setRangeLower(0);
          yAxis->setRangeUpper(yMax * 1.2 > 10 ? yMax * 1.2 : 10);
          replot();
          usingMenu = false;
        }

        //*========================================== Clear Hist
        if( selectedAction == a2 ){
          Clear();
          usingMenu = false;
        }

        //*========================================== Toggle Stat.
        if( selectedAction == a3 ){
          for( int i = 0; i < 3; i++){
            txt[i]->setVisible( !txt[i]->visible());
          }
          replot();
          usingMenu = false;
        }
        //*========================================== Rebin
        if( selectedAction == a4 ){
          QDialog dialog(this);
          dialog.setWindowTitle("Rebin histogram");

          QFormLayout layout(&dialog);

          QLabel * info = new QLabel(&dialog);
          info->setStyleSheet("color:red;");
          info->setText("This will also clear histogram!!");
          layout.addRow(info);

          QStringList nameList = {"Num. Bin", "x-Min", "x-Max"};
          QLineEdit* lineEdit[3];

          for (int i = 0; i < 3; ++i) {
              lineEdit[i] = new QLineEdit(&dialog);
              layout.addRow(nameList[i] + " : ", lineEdit[i]);
          }
          lineEdit[0]->setText(QString::number(xBin));
          lineEdit[1]->setText(QString::number(xMin));
          lineEdit[2]->setText(QString::number(xMax));

          QLabel * msg = new QLabel(&dialog);
          msg->setStyleSheet("color:red;");
          layout.addRow(msg);

          QDialogButtonBox buttonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, &dialog);
          layout.addRow(&buttonBox);

          double number[3];

          QObject::connect(&buttonBox, &QDialogButtonBox::accepted, [&]() {
            int OKcount = 0;
            bool conversionOk = true;
            for( int i = 0; i < 3; i++ ){
              number[i] = lineEdit[i]->text().toDouble(&conversionOk);
              if( conversionOk ){
                OKcount++;
              }else{
                msg->setText(nameList[i] + " is invalid.");
                return;
              }
            }

            if( OKcount == 3 ) {
              if( number[2] > number[1] ) {
                dialog.accept();
              }else{
                msg->setText(nameList[2] + " is smaller than " + nameList[1]);
              }
            }
          });
          QObject::connect(&buttonBox, &QDialogButtonBox::rejected, [&]() { dialog.reject();});

          if( dialog.exec() == QDialog::Accepted ){
            /// Direct connection on the GUI thread, so every AboutToRebin handler has returned --
            /// and any worker filling this plot has drained -- before Rebin() touches yList.
            emit AboutToRebin();
            Rebin((int)number[0], number[1], number[2]);
            emit ReBinned();
            UpdatePlot();
          }

        }

        //*========================================== Toggle line Display
        if( a6 && selectedAction == a6 ){
          QDialog dialog(this);
          dialog.setWindowTitle("Toggle lines Display");

          QFormLayout layout(&dialog);

          QVector<QCheckBox*> cbline(nData);
          for( int i = 0; i < nData; i++ ){
            cbline[i] = new QCheckBox(graph(i)->name(), &dialog);
            layout.addRow(cbline[i]);
            if( showHist[i] ) cbline[i]->setChecked(true);
          }

          QDialogButtonBox buttonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, &dialog);
          layout.addRow(&buttonBox);

          QObject::connect(&buttonBox, &QDialogButtonBox::accepted, [&]() {
            for( int i = 0; i < nData; i++ ){
              showHist[i] = cbline[i]->isChecked();
            }
            dialog.accept();
          });
          QObject::connect(&buttonBox, &QDialogButtonBox::rejected, [&]() { dialog.reject();});

          if( dialog.exec() == QDialog::Accepted ){
            UpdatePlot();
          }
        }
        //*========================================== Set Log y
        if( selectedAction == a5 ){
          if( !isLogY ){
            this->yAxis->setScaleType(QCPAxis::stLogarithmic);
            isLogY = true;
          }else{
            this->yAxis->setScaleType(QCPAxis::stLinear);
            isLogY = false;
          }
          this->replot();
        }
        //*========================================== Gate add / remove
        if( gAdd && selectedAction == gAdd ){
          AddGate(event->pos());   // center the new band under the right-click
          usingMenu = false;
        }
        if( gClr && selectedAction == gClr ){
          ClearGates();
          usingMenu = false;
        }
        if( gRen && selectedAction == gRen ){
          EditGatesDialog();
          usingMenu = false;
        }
        for( int k = 0; k < gDelActs.size(); k++ ){
          if( selectedAction == gDelActs[k] ){
            RemoveGate( gDelActs[k]->data().toInt() );
            usingMenu = false;
          }
        }
        usingMenu = false;
      }
    });

  }

  int    GetNBin() const {return xBin;}
  double GetXMin() const {return xMin;}
  double GetXMax() const {return xMax;}
  unsigned long GetTotalEntry() const {return totalEntry;}

  /// "Total Entry" stat line. The bracketed rate is INSTANTANEOUS (trailing time-average window,
  /// global "Time average" control), normalized to Hz: SingleSpectra's sample path feeds the
  /// worker's exact counts through its RateStore and pushes the windowed dN/dt back in via
  /// SetDisplayRate() -- the widget never times on its own, so the counting clock lives in
  /// exactly one place. Until the window has two samples the rate is omitted entirely.
  /// It rides in txt[1], showing/hiding with "Toggle Stat.".
  QString TotalEntryLabel(){
    const QString cnt = "Total Entry : " + QString::number(totalEntry);
    if( !haveRate ) return cnt;
    const QString r = QString::number(dispRate,'f',1);
    return cnt + " [" + r + " Hz]";
  }

  /// Called by the publish path every repaint; r < 0 means "window not filled yet".
  void SetDisplayRate(double r){
    haveRate = (r >= 0.0);
    dispRate = qMax(0.0, r);
    if( txt[1] ) txt[1]->setText(TotalEntryLabel());
  }

  int  RateIntervalMs() const { return rateIntervalMs; }
  QString RateIntervalText() const {
    return ( rateIntervalMs % 1000 == 0 ) ? QString("%1 s").arg(rateIntervalMs/1000)
                                          : QString("%1 ms").arg(rateIntervalMs);
  }
  /// MIRROR of the one GLOBAL time average (the "Time average" control next to "Rate Monitor"):
  /// every plot and the monitor window use the same trailing window; a per-plot menu would only
  /// invite numbers that look alike but do not agree. SingleSpectra::SetRateTimeBase() pushes
  /// the value to all plots; this widget only counts on it being what the store windowed with.
  void SetRateIntervalMs(int ms){
    rateIntervalMs = qBound(100, ms, 3600000);
    if( txt[1] ) txt[1]->setText(TotalEntryLabel());
  }

  //^==================== 1D gates
  //^ A gate is a shaded [x0,x1] band with two draggable edge lines. Its instantaneous rate is
  //^ derived GUI-side from the PUBLISHED bins (GateSum = sum of series-0 bins inside the band),
  //^ sampled into SingleSpectra::RateStore under key "<rateKey>/g<n>" by the publish path --
  //^ the fill worker never knows gates exist. The cost is exact at bin resolution and bounded
  //^ by the publish period; it follows the pane, so an unmounted histogram has no gate counts
  //^ (by design, for now).

  int GateCount() const { return gates.size(); }
  double GateLow(int k)  const { return (k>=0 && k<gates.size()) ? qMin(gates[k].x0, gates[k].x1) : 0.; }
  double GateHigh(int k) const { return (k>=0 && k<gates.size()) ? qMax(gates[k].x0, gates[k].x1) : 0.; }
  int  GateId(int k)     const { return (k>=0 && k<gates.size()) ? gates[k].id : -1; }
  QString GateName(int k)const { return (k>=0 && k<gates.size()) ? gates[k].name : QString(); }
  /// RateStore key. Carries the gate's NEVER-REUSED id, not its position: a middle delete must
  /// not graft the victim's rate history onto a survivor.
  QString GateKey(int k) const{
    return (k>=0 && k<gates.size()) ? rateKey + QString("/g%1").arg(gates[k].id) : QString();
  }

  int FindGate(const QString & name) const{
    for( int k = 0; k < gates.size(); k++ ) if( gates[k].name == name ) return k;
    return -1;
  }

  bool RenameGate(int k, const QString & name){
    if( k < 0 || k >= gates.size() || name.isEmpty() ) return false;
    gates[k].name = name;
    UpdateGateLabel(k);
    emit GateRenamed( GateKey(k), name );   // id (and so the key) is unchanged; only the label moves
    return true;
  }

  /// atPixel (widget coords) centers the band under the cursor; the default centers it on the
  /// axis. Width is 10 % of the axis range. All item positions are ptPlotCoords -- QCP defaults
  /// them to ptAbsolute, which would place energy values at equal pixel offsets, i.e. invisible.
  int AddGate(const QPoint & atPixel = QPoint(INT_MIN, INT_MIN), bool announce = true){
    if( gates.size() >= MaxGates ) return -1;
    const double hw = 0.05*(xMax - xMin);                       // 10 % full width
    double c = ( atPixel.x() == INT_MIN ) ? 0.5*(xMin + xMax)
                                          : xAxis->pixelToCoord(atPixel.x());
    c = qBound(xMin + hw, c, xMax - hw);

    Gate g;
    g.x0 = c - hw;
    g.x1 = c + hw;
    g.id   = gateSeq ++;                    // monotonic: colors and store keys follow the gate
    g.name = QString("Gate-%1").arg(g.id);
    g.rateLine = "(gathering...)";

    const QColor col = gateColor(g.id);
    QPen pen(col, 1.5);
    g.l0 = new QCPItemStraightLine(this);
    g.l1 = new QCPItemStraightLine(this);
    g.band = new QCPItemRect(this);
    g.band->setPen(Qt::NoPen);
    g.l0->point1->setType(QCPItemPosition::ptPlotCoords); g.l0->point2->setType(QCPItemPosition::ptPlotCoords);
    g.l1->point1->setType(QCPItemPosition::ptPlotCoords); g.l1->point2->setType(QCPItemPosition::ptPlotCoords);
    g.band->topLeft->setType(QCPItemPosition::ptPlotCoords); g.band->bottomRight->setType(QCPItemPosition::ptPlotCoords);
    g.rateTxt = new QCPItemText(this);
    g.rateTxt->setFont(QFont("Helvetica", 9));
    g.rateTxt->setPositionAlignment(Qt::AlignBottom | Qt::AlignHCenter);
    /// x rides the gate center in plot coords; y is a RATIO of the axis rect so the labels
    /// stack upward from the x axis (10% + 5%*k above it) and never collide with the legend
    /// or each other, through zoom, pan and log-y.
    g.rateTxt->position->setTypeX(QCPItemPosition::ptPlotCoords);
    g.rateTxt->position->setTypeY(QCPItemPosition::ptAxisRectRatio);

    gates.append(g);
    ApplyGateColor(gates.size()-1);
    UpdateGateLabel(gates.size()-1);
    UpdateGateGeometry();
    replot();
    /// announce=false during RestoreGate: the id/name are not final yet and the key rides
    /// the id -- announcing here would register a TEMP key no one owns.
    if( announce ) emit GateCreated( GateKey(gates.size()-1), gates.last().name );
    return gates.size() - 1;
  }

  /// Re-create a persisted gate: its saved id (which carries the COLOR and the RateStore key),
  /// name and bounds. id < 0 keeps the auto-assigned one. gateSeq is pushed past the restored
  /// id so fresh gates can never collide with it.
  /// A duplicated file (every gate twice, saved by a pre-clear LoadSetting) keeps its FIRST
  /// copy of each id and ignores the twin: two live gates sharing an id would share one
  /// RateStore key, and each SampleRates() pass would overwrite the other's sum.
  int RestoreGate(int id, const QString & name, double lo, double hi){
    if( id >= 0 )
      for( int k = 0; k < gates.size(); k++ )
        if( gates[k].id == id ) return k;
    const int k = AddGate( QPoint(INT_MIN,INT_MIN), /*announce*/false );
    if( k < 0 ) return -1;
    if( id >= 0 ){
      gates[k].id   = id;
      gateSeq       = qMax(gateSeq, id + 1);
      ApplyGateColor(k);
    }
    if( !name.isEmpty() ) gates[k].name = name;
    SetGateBounds(k, lo, hi);
    UpdateGateLabel(k);
    /// AddGate() replot()s the FRESH, centred, still-titled gate; nothing after it redraws,
    /// so with the replot clock idle (ACQ stopped) that interim frame is what the user sees:
    /// a "ghost Gate-N" floating at the centre until any manual replot. Show the final one.
    replot();
    /// Announce only NOW, under the FINAL key (the id renumber above moved it) and name.
    emit GateCreated( GateKey(k), gates[k].name );
    return k;
  }

  void ApplyGateColor(int k){
    const QColor col = gateColor(gates[k].id);
    QPen pen(col, 1.5);
    gates[k].l0->setPen(pen);
    gates[k].l1->setPen(pen);
    QColor fill = col; fill.setAlpha(30);
    gates[k].band->setBrush(fill);
    gates[k].rateTxt->setColor(col.darker(140));
  }

  /// Delete live gate at position k. Index shifts are safe here (the 2D cut list needed
  /// tombstones because its entries reference shared QCustomPlot list indices; gates hold
  /// their item pointers directly), but the RATE key must not shift -- see GateKey().
  /// Emits GateDeleted so SingleSpectra can drop the store history.
  void RemoveGate(int k){
    if( k < 0 || k >= gates.size() ) return;
    emit GateDeleted(GateKey(k));
    Gate & g = gates[k];
    removeItem(g.l0); removeItem(g.l1); removeItem(g.band); removeItem(g.rateTxt); // removeItem deletes
    gates.remove(k);
    if( dragGate == k ) dragGate = -1;
    else if( dragGate > k ) dragGate --;
    replot();
  }
  void RemoveGate(const QString & name){ RemoveGate(FindGate(name)); }
  void RemoveGate(){ RemoveGate(gates.size()-1); }   // legacy "remove the last one"

  void ClearGates(){
    while( !gates.isEmpty() ) RemoveGate(0);
  }

  /// One row per live gate: [color] Name | Low | High, boundary values in AXIS units.
  /// OK applies everything: rename (empty keeps the old name), bounds swap if inverted and
  /// clamp to the axis domain. Replaces the old names-only dialog.
  void EditGatesDialog(){
    if( gates.isEmpty() ) return;

    QDialog dialog(this);
    dialog.setWindowTitle("Edit Gates");
    QFormLayout layout(&dialog);

    QList<QLineEdit *> nameLe, loLe, hiLe;
    for( int k = 0; k < gates.size(); k++ ){
      QHBoxLayout * row = new QHBoxLayout();
      nameLe.append(new QLineEdit(&dialog)); nameLe.last()->setText( gates[k].name );
      loLe .append(new QLineEdit(&dialog));  loLe .last()->setText( QString::number(qMin(gates[k].x0,gates[k].x1),'g',10) );
      hiLe .append(new QLineEdit(&dialog));  hiLe .last()->setText( QString::number(qMax(gates[k].x0,gates[k].x1),'g',10) );
      row->addWidget(nameLe.last());
      row->addWidget(new QLabel("-", &dialog));
      row->addWidget(loLe.last());
      row->addWidget(new QLabel("to", &dialog));
      row->addWidget(hiLe.last());
      layout.addRow( gateColorName(gates[k].id) + " :", row );
    }
    layout.addRow(new QLabel("Boundaries in axis units; inverted pairs are swapped, out-of-range clamped.", &dialog));

    QDialogButtonBox buttonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, &dialog);
    layout.addRow(&buttonBox);
    connect(&buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(&buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if( dialog.exec() != QDialog::Accepted ) return;

    for( int k = 0; k < gates.size(); k++ ){
      RenameGate(k, nameLe[k]->text());
      bool okLo = false, okHi = false;
      const double lo = loLe[k]->text().toDouble(&okLo);
      const double hi = hiLe[k]->text().toDouble(&okHi);
      if( okLo && okHi ) SetGateBounds(k, lo, hi);   // unparsable pair: leave the gate where it is
    }
    replot();
  }

  /// Cumulative in-range entries inside gate k, from the published series-0 bins.
  unsigned long GateSum(int k) const{
    if( k < 0 || k >= gates.size() || dX <= 0. ) return 0;
    const double lo = qMin(gates[k].x0, gates[k].x1);
    const double hi = qMax(gates[k].x0, gates[k].x1);
    const int b0 = qBound(0, (int)qFloor((lo - xMin)/dX), yList[0].count()-1);
    const int b1 = qBound(0, (int)qFloor((hi - xMin)/dX), yList[0].count()-1);
    unsigned long s = 0;
    for( int b = b0; b <= b1; b++ ) s += (unsigned long)yList[0][b];
    return s;
  }

  void SetGateRate(int k, bool valid, double r){
    if( k < 0 || k >= gates.size() ) return;
    gates[k].rateLine = valid ? QString("%1 Hz").arg(r, 0, 'f', 1)
                              : QString("(gathering...)");
    UpdateGateLabel(k);
    // no replot here: every publish path that calls this replots the pane right after
  }

  /// Set the band edges in AXIS units. Inverted bounds are swapped, values are clamped to
  /// [xMin,xMax]; feeds the "Edit Gates..." dialog and keeps the x0<x1 label invariant.
  void SetGateBounds(int k, double lo, double hi){
    if( k < 0 || k >= gates.size() || dX <= 0. ) return;
    if( hi < lo ) qSwap(lo, hi);
    gates[k].x0 = qBound(xMin, lo, xMax);
    gates[k].x1 = qBound(xMin, hi, xMax);
    UpdateGateGeometry();
    emit GateBoundsChanged( GateKey(k) );
  }

  void UpdateGateLabel(int k){
    if( k < 0 || k >= gates.size() ) return;
    gates[k].rateTxt->setText( gates[k].name + "\n" + gates[k].rateLine );
  }

  /// Vertical span of the band and the rate label follow the y range; the straight lines span
  /// the plot on their own. Called from beforeReplot so zoom/pan keeps everything glued.
  void UpdateGateGeometry(){
    const QCPRange yr = yAxis->range();
    for( int k = 0; k < gates.size(); k++ ){
      Gate & g = gates[k];
      g.l0->point1->setCoords(g.x0, 0.); g.l0->point2->setCoords(g.x0, 1.);
      g.l1->point1->setCoords(g.x1, 0.); g.l1->point2->setCoords(g.x1, 1.);
      g.band->topLeft    ->setCoords(qMin(g.x0,g.x1), yr.upper);
      g.band->bottomRight->setCoords(qMax(g.x0,g.x1), yr.lower);
      g.rateTxt->position->setCoords(0.5*(g.x0+g.x1), 1.0 - (0.10 + 0.05*k));
    }
  }

  static QColor gateColor(int id){
    static const QColor c[MaxGates] = { QColor(200, 0,   0),   QColor(170, 0, 170),
                                        QColor(0, 120,   0),   QColor(230, 130, 0) };
    return c[id % MaxGates];
  }
  static QString gateColorName(int id){
    static const char * n[MaxGates] = { "red", "magenta", "green", "orange" };
    return QString(n[id % MaxGates]);
  }

  void SetColor(QColor color, unsigned short ID = 0) {
    // DebugPrint("%s", "Histogram1D");
    graph(ID)->setPen(QPen(color));
    QColor haha = color;
    haha.setAlpha(20);
    graph(ID)->setBrush(QBrush(haha));
  }
  void AddDataList(QString title, QColor color){
    if( nData >= MaxNHist ){
      printf("Histogram1D::AddDataList: maximum number of histograms (%d) reached.\n", MaxNHist);
      return;
    }
    nData ++;
    addGraph();
    graph(nData - 1)->setName(title);
    graph(nData - 1)->setLineStyle(QCPGraph::lsStepCenter);
    SetColor(color, nData-1);
    yList[nData-1].clear();
    for( int i = 0; i < xList.count(); i++) yList[nData-1].append(0);
  }

  void UpdatePlot(){
    // DebugPrint("%s", "Histogram1D");
    for( int ID = 0 ; ID < nData; ID ++) {
      graph(ID)->setVisible(showHist[ID]);
      graph(ID)->setData(xList, yList[ID]);
    }
    xAxis->setRangeLower(xMin);
    xAxis->setRangeUpper(xMax);
    yAxis->setRangeLower(0);
    yAxis->setRangeUpper(yMax * 1.2 > 10 ? yMax * 1.2 : 10);
    replot();
  }

  void Clear(){
    // DebugPrint("%s", "Histogram1D");
    for( int ID = 0 ; ID < nData; ID ++) {
      for( int i = 0; i < xList.count(); i++) yList[ID][i] = 0;
    }
    yMax = 0;
    totalEntry = 0;
    underFlow = 0;
    overFlow = 0;
    haveRate = false;   // the store re-seeds after its decrease-detected reset; no stale rate
    txt[0]->setText("Under Flow : 0");
    txt[1]->setText(TotalEntryLabel());
    txt[2]->setText("Over Flow : 0");
    UpdatePlot();
  }
  
  void SetLineTitle(QString title, int lineID = 0) { graph(lineID)->setName(title);  }
  void SetXTitle(QString xTitle) { xAxis->setLabel(xTitle);}

  void Rebin(int xbin, double xmin, double xmax){
    // DebugPrint("%s", "Histogram1D");
    xMin = xmin;
    xMax = xmax;
    xBin = xbin;
    if( xBin > 1000){
      printf("Histogram1D::Rebin: xBin capped at 1000 (requested %d).\n", xBin);
      xBin = 1000;
    }

    dX = (xMax - xMin)/(xBin);

    xList.clear();
    for( int i = 0 ; i < nData ; i ++) yList[i].clear();

    /// One vertex per bin, at the bin CENTRE, and the graph draws the steps (lsStepCenter, set
    /// where each graph is added). This used to emit two x samples per bin edge -- one of them
    /// nudged down by a millionth of a bin -- and carry the count in two slots, so every index in
    /// the class was written as 2*bin+1 and Fill() incremented twice per hit on the hottest path
    /// in online histogramming. QCustomPlot has rendered steps natively all along; the default
    /// lsLine was what the doubling was faking.
    ///
    /// The centres, not the edges: lsStepCenter puts the step boundary midway between two
    /// vertices, which for a uniform binning is exactly the bin edge. The visible difference is
    /// at the two ends, where the outer half of the first and last bin is no longer drawn.
    for( int i = 0; i < xBin; i ++ ){
      xList.append(xMin + (i + 0.5)*dX);
      for( int ID = 0 ; ID < nData; ID ++ ) yList[ID].append(0);
    }

    yMax = 0;

    totalEntry = 0;
    underFlow = 0;
    overFlow = 0;
    haveRate = false;

    /// Gates are plot-coordinate objects: keep them inside the new axis domain.
    for( Gate & g : gates ){
      g.x0 = qBound(xMin, g.x0, xMax);
      g.x1 = qBound(xMin, g.x1, xMax);
    }

    /// Changing the geometry un-zooms: a view pinned to the old range shows a blank or
    /// cropped plot under the new binning. Full range is also what UpdatePlot keeps, so the
    /// view does not drift back.
    xAxis->setRange(xMin, xMax);

    if( txt[0] ) txt[0]->setText("Under Flow : 0");
    if( txt[1] ) txt[1]->setText(TotalEntryLabel());
    if( txt[2] ) txt[2]->setText("Over Flow : 0");
  }

  void Fill(double value, unsigned int ID = 0){
    // DebugPrint("%s", "Histogram1D");
    /// SetBinContents has always checked this; Fill never did, and yList is a plain [MaxNHist].
    if( ID >= (unsigned int) nData ) return;
    if( ID == 0 ){
      /// totalEntry counts only in-range entries (the plotted "Total Entry"); under/over are the
      /// out-of-range rejects, matching the worker paths (AddCount1D / Fill1).
      if( value < xMin ) {
        underFlow ++;
        txt[0]->setText("Under Flow : "+ QString::number(underFlow));
        return;
      }
      if( value >= xMax ) {
        overFlow ++;
        txt[2]->setText("Over Flow : "+ QString::number(overFlow));
        return;
      }
      totalEntry ++;
      txt[1]->setText(TotalEntryLabel());
    }else{
      if( value < xMin || value >= xMax ) return;
    }

    int bin = qFloor((value - xMin)/dX);
    if( bin < 0 || bin >= yList[ID].count() ) return;

    yList[ID][bin] += 1;

    if( showHist[ID] && yList[ID][bin] > yMax ) yMax = yList[ID][bin];
  }

  /// Replace the whole histogram in one go, from counts accumulated elsewhere.
  ///
  /// This exists so a worker thread never has to call Fill(). Fill() writes to the QCPItemText
  /// stat labels, and a QString assignment races the GUI thread's replot()/draw() reading them —
  /// the refcount is atomic but the d-pointer store is not, so the reader can be left holding a
  /// freed buffer. The online analyzer therefore accumulates into plain count arrays on its worker
  /// and calls this from the GUI thread instead.
  ///
  /// GUI THREAD ONLY. `y` holds `n` bin contents; extra bins are zeroed, and n is clamped to xBin.
  ///
  /// With more than one series (PSD adds a short-gate graph) call this for ID 0 first, then in
  /// ascending ID. yMax is only reset on ID 0 and grows from there, so the y-range ends up
  /// covering every series -- resetting it on each call would leave the axis scaled to whichever
  /// series happened to be published last. This matches Fill(), which also only ever grows yMax;
  /// Clear() and Rebin() are what zero it.
  void SetBinContents(const uint32_t * y, int n, unsigned long total,
                      unsigned long under, unsigned long over, unsigned int ID = 0){

    if( ID >= (unsigned int) nData || y == nullptr ) return;
    if( n > xBin ) n = xBin;

    if( ID == 0 ) yMax = 0;
    for( int b = 0; b < yList[ID].count(); b++ ){
      const double v = ( b < n ) ? (double) y[b] : 0.0;
      yList[ID][b] = v;
      if( showHist[ID] && v > yMax ) yMax = v;
    }

    totalEntry = total;
    underFlow  = under;
    overFlow   = over;
    txt[0]->setText("Under Flow : " + QString::number(underFlow));
    txt[1]->setText(TotalEntryLabel());
    txt[2]->setText("Over Flow : " + QString::number(overFlow));
  }

  void Print(unsigned int ID = 0){
    for( int i = 0; i < xList.count(); i++){
      printf("%f  %f\n", xList[i], yList[ID][i]);
    }
  }

  void SetRateKey(const QString & k){ rateKey = k; }
  QString RateKey() const { return rateKey; }

protected:
  /// Gate grabbing must happen BEFORE QCustomPlot's own press handling starts a zoom rect,
  /// so it is an override, not a signal connect (signals fire after the internal logic).
  void mousePressEvent(QMouseEvent * event) override{
    if( event->button() == Qt::LeftButton && !usingMenu && TryGrabGate(event->pos()) ){
      setSelectionRectMode(QCP::SelectionRectMode::srmNone);
      return;  // swallowed: no zoom rect starts
    }
    QCustomPlot::mousePressEvent(event);
  }
  void mouseMoveEvent(QMouseEvent * event) override{
    if( dragGate >= 0 ){
      DragGateTo(event);
      replot();
      return;
    }
    QCustomPlot::mouseMoveEvent(event);   // emits the mouseMove signal -> bin/value tooltip

    /// Hovering a gate edge reports that EDGE's bin (shown last so it wins over the generic
    /// bin-under-cursor tooltip).
    for( int k = 0; k < gates.size(); k++ ){
      const double p0 = xAxis->coordToPixel(gates[k].x0);
      const double p1 = xAxis->coordToPixel(gates[k].x1);
      const bool e0 = qAbs(event->pos().x() - p0) <= 6;
      const bool e1 = !e0 && qAbs(event->pos().x() - p1) <= 6;
      if( e0 || e1 ){
        QToolTip::showText( event->globalPosition().toPoint(), GateEdgeTip(gates[k], e1), this );
        break;
      }
    }
  }
  void mouseReleaseEvent(QMouseEvent * event) override{
    if( dragGate >= 0 ){
      emit GateBoundsChanged( GateKey(dragGate) );   // the saved bounds just moved
      dragGate = -1;
      QToolTip::hideText();
      setSelectionRectMode(QCP::SelectionRectMode::srmNone);
      replot();
      return;  // swallow: a gate drag must not trigger a zoom of its sliver rect
    }
    QCustomPlot::mouseReleaseEvent(event);
  }

signals:
  /// Both ONLY for the right-click rebin.
  ///
  /// Rebin() clears and re-appends yList, which frees the QVector buffer. An owner that
  /// accumulates into this plot from another thread has to be stopped BEFORE that happens, so it
  /// gets AboutToRebin() first and ReBinned() once the new binning is in place. Emitting only
  /// afterwards would be useless: by then the buffer the other thread is indexing is already gone.
  void AboutToRebin();
  void ReBinned();

  /// A gate was deleted; SingleSpectra drops its RateStore history so the key does not linger.
  void GateDeleted(const QString & storeKey);

  /// A gate's birth / rename. Key follows the gate ID (stable across renames); the owner
  /// registers the key in the RateStore and refreshes the monitor picker from these -- with
  /// the DAQ idle, a drawn or loaded gate must be listable without ever having been sampled.
  void GateCreated(const QString & key, const QString & name);
  void GateRenamed(const QString & key, const QString & name);
  /// Bounds moved: from the drag release and from the Edit dialog (via SetGateBounds). Emitted
  /// by SetGateBounds itself, so every path that changes a stored value announces it.
  void GateBoundsChanged(const QString & key);

private:
  double xMin, xMax, dX;
  int xBin;

  double yMax;

  unsigned long totalEntry;   ///< in-range entries only (the plotted "Total Entry"); under/over are the rejects
  unsigned long underFlow;
  unsigned long overFlow;

  //^---- instantaneous rate (see RateStore in SingleSpectra: the window math lives there)
  QString rateKey;            ///< my channel name in the store, set by SingleSpectra at creation
  int  rateIntervalMs = 1000; ///< right-click adjustable integration window
  bool haveRate = false;
  double dispRate = 0.0;

  //^---- 1D gates
  struct Gate{
    double x0 = 0, x1 = 0;
    int  id = -1;                 ///< monotonic per plot, NEVER reused: colors and RateStore keys ride it
    QString name;                 ///< default "Gate-<id>", renameable via the menu
    QString rateLine;             ///< last rate text; the label is rebuilt as name + rateLine
    QCPItemStraightLine * l0 = nullptr;
    QCPItemStraightLine * l1 = nullptr;
    QCPItemRect         * band = nullptr;
    QCPItemText         * rateTxt = nullptr;
  };
  QVector<Gate> gates;
  int gateSeq = 0;   ///< next gate id; ids survive deletes, so a middle delete renumbers nothing
  static const int MaxGates = 4;
  int dragGate = -1;   ///< -1 = none; edges: 0 = x0, 1 = x1
  int dragEdge = 0;

  /// Gate-edge grab / drag, driven from the mouse signal connects in the ctor.
  bool TryGrabGate(const QPoint & pos){
    for( int k = 0; k < gates.size(); k++ ){
      const double p0 = xAxis->coordToPixel(gates[k].x0);
      const double p1 = xAxis->coordToPixel(gates[k].x1);
      if( qAbs(pos.x() - p0) <= 6 ){ dragGate = k; dragEdge = 0; return true; }
      if( qAbs(pos.x() - p1) <= 6 ){ dragGate = k; dragEdge = 1; return true; }
    }
    return false;
  }
  /// Tooltip text for one edge: which bin it sits in and where, in axis units.
  QString GateEdgeTip(const Gate & g, bool edge) const{
    const double x = edge ? g.x1 : g.x0;
    const int bin = qBound( 0, (int)qFloor((x - xMin)/dX), yList[0].count()-1 );
    return QString("%1  edge %2\nbin %3 @ %4").arg(g.name).arg(edge?"B":"A")
                                              .arg(bin).arg(x, 0, 'g', 6);
  }

  void DragGateTo(const QMouseEvent * event){
    if( dragGate < 0 ) return;
    double & x = (dragEdge == 0) ? gates[dragGate].x0 : gates[dragGate].x1;
    x = qBound(xMin, xAxis->pixelToCoord(event->pos().x()), xMax);
    QToolTip::showText( event->globalPosition().toPoint(), GateEdgeTip(gates[dragGate], dragEdge), this );
    UpdateGateGeometry();
  }

  bool isLogY;

  unsigned short nData;
  QVector<double> xList;
  QVector<double> yList[MaxNHist];

  QCPItemText * txt[3];

  bool usingMenu;

  bool showHist[MaxNHist];


};

#endif