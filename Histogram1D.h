#ifndef HISTOGRAM_1D_H
#define HISTOGRAM_1D_H

#include "qcustomplot.h"
#include "macro.h"

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
    txt[1]->setText("Total Entry : 0");
    txt[2]->setText("Over Flow : 0");

    usingMenu = false;

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
        usingMenu = false;
      }
    });

  }

  int    GetNBin() const {return xBin;}
  double GetXMin() const {return xMin;}
  double GetXMax() const {return xMax;}

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
    txt[0]->setText("Under Flow : 0");
    txt[1]->setText("Total Entry : 0");
    txt[2]->setText("Over Flow : 0");
    totalEntry = 0;
    underFlow = 0;
    overFlow = 0;
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

    if( txt[0] ) txt[0]->setText("Under Flow : 0");
    if( txt[1] ) txt[1]->setText("Total Entry : 0");
    if( txt[2] ) txt[2]->setText("Over Flow : 0");
  }

  void Fill(double value, unsigned int ID = 0){
    // DebugPrint("%s", "Histogram1D");
    /// SetBinContents has always checked this; Fill never did, and yList is a plain [MaxNHist].
    if( ID >= (unsigned int) nData ) return;
    if( ID == 0 ){
      totalEntry ++;
      txt[1]->setText("Total Entry : "+ QString::number(totalEntry));

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
    txt[1]->setText("Total Entry : "+ QString::number(totalEntry));
    txt[2]->setText("Over Flow : " + QString::number(overFlow));
  }

  void Print(unsigned int ID = 0){
    for( int i = 0; i < xList.count(); i++){
      printf("%f  %f\n", xList[i], yList[ID][i]);
    }
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

private:
  double xMin, xMax, dX;
  int xBin;

  double yMax;

  unsigned long totalEntry;
  unsigned long underFlow;
  unsigned long overFlow;

  bool isLogY;

  unsigned short nData;
  QVector<double> xList;
  QVector<double> yList[MaxNHist];

  QCPItemText * txt[3];

  bool usingMenu;

  bool showHist[MaxNHist];


};

#endif