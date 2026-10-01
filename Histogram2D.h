#ifndef HISTOGRAM_2D_H
#define HISTOGRAM_2D_H

#include "qcustomplot.h"
#include "macro.h"
#include <QFileDialog>
#include <QDir>
#include <limits>

inline const QList<QPair<QColor, QString>> colorCycle = { {QColor(Qt::red), "Red"},
                                                         {QColor(Qt::blue), "Blue"},
                                                         {QColor(Qt::darkGreen), "Dark Green"},
                                                         {QColor(Qt::darkCyan), "Dark Cyan"}, 
                                                         {QColor(Qt::darkYellow), "Dark Yellow"}, 
                                                         {QColor(Qt::magenta),  "Magenta"},
                                                         {QColor(Qt::darkMagenta), "Dark Magenta"},
                                                         {QColor(Qt::gray), "Gray"}}; 


//^==============================================
//^==============================================
class Histogram2D : public QCustomPlot{
  Q_OBJECT

public:
  Histogram2D(QString title, QString xLabel, QString yLabel, 
              int xbin, double xmin, double xmax, 
              int ybin, double ymin, double ymax, 
              QWidget * parent = nullptr,
              QString defaultPath = QDir::homePath());

  void SetXTitle(QString xTitle) { xAxis->setLabel(xTitle);}
  void SetYTitle(QString yTitle) { yAxis->setLabel(yTitle);}
  void Rebin(int xbin, double xmin, double xmax, int ybin, double ymin, double ymax);
  void RebinY(int ybin, double ymin, double ymax);

  void SetChannelMap(bool onOff, int tickStep = 1) { isChannelMap = onOff; this->tickStep = tickStep;}

  void UpdatePlot(){ colorMap->rescaleDataRange(true); replot(); }
  void Clear(); // Clear Data and histrogram

  void Fill(double x, double y);

  /// Replace the whole colormap in one go, from counts accumulated elsewhere. See the long note on
  /// Histogram1D::SetBinContents: Fill() mutates QCPItemText and walks cutList, both of which race
  /// the GUI thread, so a worker must never call it. GUI THREAD ONLY.
  ///
  /// `z` is row-major, nx*ny entries, indexed z[ix*ny + iy]. A zero count is written as NaN so it
  /// renders as background, the same as an unfilled cell.
  void SetCellContents(const uint32_t * z, int nx, int ny, unsigned long total,
                       unsigned long under, unsigned long over);

  /// The cell grid this widget actually renders.
  ///
  /// Anything that accumulates counts for SetCellContents() must take its geometry from here, and
  /// index with CoordToCell(), or the plot is drawn on the wrong scale. Two things conspire:
  /// Rebin() stores xBin = requested + 2, and QCPColorMapData::coordToCell spreads [xMin,xMax]
  /// over (nx - 1) intervals rather than nx. An accumulator that bins over `requested` cells and
  /// writes cell i therefore disagrees with where the widget draws cell i by nx/(nx+1) -- 0.1% at
  /// 1000 bins, but ~6% plus two permanently empty columns on a 16-channel map.
  struct CellGeom {
    int    nx = 0, ny = 0;            ///< INCLUDING the two guard cells Rebin() adds
    double xLo = 0, xHi = 0, yLo = 0, yHi = 0;
  };
  CellGeom GetCellGeom() const { return { xBin, yBin, xMin, xMax, yMin, yMax }; }

  /// Exactly QCPColorMapData::coordToCell's arithmetic, but off a cached CellGeom so a worker
  /// thread can bin without touching the widget at all. False when the point falls outside the
  /// grid, is not finite, or the geometry is degenerate -- callers count those as out-of-range.
  static bool CoordToCell(const CellGeom & g, double x, double y, int * ix, int * iy);

  void DrawCut();
  void ClearAllCuts();

  /// One cut. This used to be five index-aligned QLists plus three loose scalars, which the
  /// delete path had to keep in step by hand and which the three accessors below handed out
  /// separately, leaving every caller to re-correlate them by index.
  ///
  /// A deleted cut is tombstoned, not erased: the remaining entries' indices must not move,
  /// because the delete menu carries them in QAction::setData and the QCustomPlot item and
  /// plottable indices are renumbered relative to it. An empty poly is what marks one.
  struct Cut {
    QPolygonF poly;            ///< empty once deleted
    QString   name;
    int       entries     = 0; ///< hits inside the polygon; -1 once deleted
    int       id          = -1;///< only ever increasing, picks the colour out of colorCycle
    int       textID      = -1;///< index into this plot's QCPAbstractItem list
    int       plottableID = -1;///< index into this plot's plottable list
  };

  const QList<Cut> & GetCuts() const { return cutList; } // may contain deleted (empty-poly) entries
  void PrintCutEntry() const;

  double GetXNBin() const {return xBin;}
  double GetXMin()  const {return xMin;}
  double GetXMax()  const {return xMax;}
  double GetYNBin() const {return yBin;}
  double GetYMin()  const {return yMin;}
  double GetYMax()  const {return yMax;}

  void SaveCuts(QString cutFileName);
  void LoadCuts(QString cutFileName);

signals:
  /// Same contract as the Histogram1D pair. Rebin() reaches QCPColorMapData::setSize(), which does
  /// delete[] mData, so an owner filling this plot from another thread must be stopped before it.
  /// The isBusy flag is not enough on its own: a worker can already be past Fill()'s isBusy check
  /// and inside setCell() by the time the array is freed.
  void AboutToRebin();
  void ReBinned();

private:
  double xMin, xMax, yMin, yMax;
  int xBin, yBin;

  bool isChannelMap; 
  int tickStep;
  bool isLogZ;

  QCPColorMap * colorMap;
  QCPColorScale *colorScale;

  bool usingMenu;

  int entry[3][3]; // overflow counter, entrt[1][1] is entry in the plot;

  QCPItemRect * box[3][3];
  QCPItemText * txt[3][3];
  
  QPolygonF tempCut;
  int tempCutID; // only incresing;
  int numCut;
  QList<Cut> cutList;
  /// Turn tempCut into a committed Cut: draw its curve, place its label, append it. The three
  /// call sites (finishing a cut by double-click, and the two end-of-polygon branches in
  /// LoadCuts) were byte-for-byte copies of each other, except that one of them was missing the
  /// empty-polygon guard.
  void CommitCut(const QString & name);
  bool isDrawCut;
  int lastPlottableID;

  QCPItemLine * line;
  double oldMouseX = 0.0, oldMouseY = 0.0;

  bool isBusy;

  void rightMouseClickMenu(QMouseEvent * event);
  void rightMouseClickRebin();

  QString settingPath;

};

//^###############################################
//^###############################################

inline Histogram2D::Histogram2D(QString title, QString xLabel, QString yLabel, int xbin, double xmin, double xmax, int ybin, double ymin, double ymax, QWidget * parent, QString defaultPath) : QCustomPlot(parent){
  // DebugPrint("%s", "Histogram2D");

  settingPath = defaultPath;
  for( int i = 0; i < 3; i ++ ){
    for( int j = 0; j < 3; j ++ ){
      box[i][j] = nullptr;
      txt[i][j] = nullptr;       
    }
  }

  isChannelMap = false;
  tickStep = 1; // only used when isChannelMap = true
  isLogZ = false;

  axisRect()->setupFullAxesBox(true);
  xAxis->setLabel(xLabel);
  yAxis->setLabel(yLabel);

  colorMap = new QCPColorMap(xAxis, yAxis);
  Rebin(xbin, xmin, xmax, ybin, ymin, ymax);
  colorMap->setInterpolate(false);

  QCPTextElement *titleEle = new QCPTextElement(this, title, QFont("sans", 12));
  plotLayout()->insertRow(0);
  plotLayout()->addElement(0, 0, titleEle);

  colorScale = new QCPColorScale(this);
  plotLayout()->addElement(1, 1, colorScale); 
  colorScale->setType(QCPAxis::atRight); 
  colorMap->setColorScale(colorScale);


  QCPColorGradient color;
  color.setNanHandling(QCPColorGradient::NanHandling::nhNanColor);
  color.setNanColor(QColor(0,0,0,0));
  color.clearColorStops();
  // color.setColorStopAt( 0.0, QColor("white" ));
  color.setColorStopAt( 0.0, QColor("purple" ));
  color.setColorStopAt( 0.2, QColor("blue"));
  color.setColorStopAt( 0.4, QColor("cyan"));
  color.setColorStopAt( 0.6, QColor("green"));
  color.setColorStopAt( 0.8, QColor("yellow"));
  color.setColorStopAt( 1.0, QColor("red"));
  colorMap->setGradient(color);

  double xPosStart = 0.02;
  double xPosStep = 0.07;
  double yPosStart = 0.02;
  double yPosStep = 0.05;

  for( int i = 0; i < 3; i ++ ){
    for( int j = 0; j < 3; j ++ ){
      box[i][j] = new QCPItemRect(this);

      box[i][j]->topLeft->setType(QCPItemPosition::ptAxisRectRatio);
      box[i][j]->topLeft->setCoords(xPosStart + xPosStep*i, yPosStart + yPosStep*j);
      box[i][j]->bottomRight->setType(QCPItemPosition::ptAxisRectRatio);
      box[i][j]->bottomRight->setCoords(xPosStart + xPosStep*(i+1), yPosStart + yPosStep*(j+1));

      txt[i][j] = new QCPItemText(this);
      txt[i][j]->setPositionAlignment(Qt::AlignLeft);
      txt[i][j]->position->setType(QCPItemPosition::ptAxisRectRatio);
      txt[i][j]->position->setCoords(xPosStart + xPosStep/2 + xPosStep*i, yPosStart + yPosStep*j);
      txt[i][j]->setText("0");
      txt[i][j]->setFont(QFont("Helvetica", 9));

    }
  }

  cutList.clear();

  rescaleAxes();

  usingMenu = false;
  isDrawCut = false;
  tempCutID = -1;
  numCut = 0;
  lastPlottableID = -1;

  line = new QCPItemLine(this);
  line->setPen(QPen(Qt::gray, 1, Qt::DashLine));
  line->setVisible(false);

  isBusy = false;

  connect(this, &QCustomPlot::mouseMove, this, [=](QMouseEvent *event){
    double x = xAxis->pixelToCoord(event->pos().x());
    double y = yAxis->pixelToCoord(event->pos().y());
    int xI, yI;
    colorMap->data()->coordToCell(x, y, &xI, &yI);
    double z = colorMap->data()->cell(xI, yI);

    QString coordinates = QString("X: %1, Y: %2, Z: %3").arg(x).arg(y).arg(z);
    QToolTip::showText(event->globalPosition().toPoint(), coordinates, this);

    //when drawing cut, show dashhed line
    if( isDrawCut && tempCut.size() > 0  ){ 
      line->end->setCoords(x,y); 
      line->setVisible(true); 
      replot();
    }

  });

  connect(this, &QCustomPlot::mousePress, this, [=](QMouseEvent * event){
    
    if (event->button() == Qt::LeftButton && !usingMenu && !isDrawCut){
      setSelectionRectMode(QCP::SelectionRectMode::srmZoom);
    }

    if (event->button() == Qt::LeftButton && isDrawCut){

      oldMouseX = xAxis->pixelToCoord(event->pos().x());
      oldMouseY = yAxis->pixelToCoord(event->pos().y());

      tempCut.push_back(QPointF(oldMouseX,oldMouseY));

      line->start->setCoords(oldMouseX, oldMouseY);
      line->end->setCoords(oldMouseX, oldMouseY);
      line->setVisible(true);

      DrawCut();
    }

    //^================= right click
    if (event->button() == Qt::RightButton) rightMouseClickMenu(event);
  });

  //connect( this, &QCustomPlot::mouseDoubleClick, this, [=](QMouseEvent *event){
  connect( this, &QCustomPlot::mouseDoubleClick, this, [=](){
    if( isDrawCut) {
      isDrawCut = false;
      line->setVisible(false);
      /// Nothing clicked yet, so there is no first vertex to close the polygon on -- tempCut[0]
      /// on an empty polygon is out of bounds. The mouseMove handler above already knows
      /// isDrawCut can be true with tempCut empty.
      if( !tempCut.isEmpty() ){
        tempCut.push_back(tempCut[0]);
        CommitCut("Cut-" + QString::number(cutList.count()));
      }
      replot();
    }
  });

  connect(this, &QCustomPlot::mouseRelease, this, [=](){

  });
}

inline bool Histogram2D::CoordToCell(const CellGeom & g, double x, double y, int * ix, int * iy){

  if( g.nx < 2 || g.ny < 2 ) return false;
  if( g.xHi == g.xLo || g.yHi == g.yLo ) return false;
  if( !std::isfinite(x) || !std::isfinite(y) ) return false;

  /// Bounded in double before the cast: a point far outside the range would otherwise overflow
  /// the int conversion, which is undefined rather than merely wrong.
  const double fx = (x - g.xLo)/(g.xHi - g.xLo)*(g.nx - 1) + 0.5;
  const double fy = (y - g.yLo)/(g.yHi - g.yLo)*(g.ny - 1) + 0.5;
  if( fx < 0 || fx >= g.nx || fy < 0 || fy >= g.ny ) return false;

  if( ix ) *ix = (int) fx;
  if( iy ) *iy = (int) fy;
  return true;
}

inline void Histogram2D::SetCellContents(const uint32_t * z, int nx, int ny, unsigned long total,
                                         unsigned long under, unsigned long over){

  if( isBusy || z == nullptr ) return;

  /// xBin/yBin include the two guard bins Rebin() adds, so only the inner cells are ours.
  if( nx > xBin ) nx = xBin;
  if( ny > yBin ) ny = yBin;

  for( int ix = 0; ix < xBin; ix++ ){
    for( int iy = 0; iy < yBin; iy++ ){
      const uint32_t c = ( ix < nx && iy < ny ) ? z[ix*ny + iy] : 0u;
      colorMap->data()->setCell(ix, iy, c ? (double) c : std::numeric_limits<double>::quiet_NaN());
    }
  }

  entry[1][1] = (int) total;
  entry[0][1] = (int) under;
  entry[2][1] = (int) over;
  txt[1][1]->setText(QString::number(entry[1][1]));
  txt[0][1]->setText(QString::number(entry[0][1]));
  txt[2][1]->setText(QString::number(entry[2][1]));
}

inline void Histogram2D::Fill(double x, double y){
  // DebugPrint("%s", "Histogram2D");
  if( isBusy ) return;
  int xIndex, yIndex;
  colorMap->data()->coordToCell(x, y, &xIndex, &yIndex);
  //printf("%f,  %d  %d| %f, %d %d\n", x, xIndex, xBin, y, yIndex, yBin);

  int xk = 1, yk = 1;
  if( xIndex < 0 ) xk = 0;
  if( xIndex >= xBin ) xk = 2;
  if( yIndex < 0 ) yk = 2;
  if( yIndex >= yBin ) yk = 0;
  entry[xk][yk] ++;

  txt[xk][yk]->setText(QString::number(entry[xk][yk]));

  if( xk == 1 && yk == 1 ) {
    double value = colorMap->data()->cell(xIndex, yIndex);
    if( std::isnan(value) ){
      colorMap->data()->setCell(xIndex, yIndex, 1);
    }else{
      colorMap->data()->setCell(xIndex, yIndex, value + 1);
    }

    for( Cut & cut : cutList ){
      if( cut.poly.isEmpty() ) continue;
      if( cut.poly.containsPoint(QPointF(x,y), Qt::OddEvenFill) ) cut.entries ++;
    }
  }
}

inline  void Histogram2D::Rebin(int xbin, double xmin, double xmax, int ybin, double ymin, double ymax){
  // DebugPrint("%s", "Histogram2D");
  xMin = xmin;
  xMax = xmax;
  yMin = ymin;
  yMax = ymax;
  xBin = xbin + 2;
  yBin = ybin + 2;

  if( xBin > 1002) xBin = 1002;
  if( yBin > 1002) yBin = 1002;

  colorMap->data()->clear();
  colorMap->data()->setSize(xBin, yBin);
  colorMap->data()->setRange(QCPRange(xMin, xMax), QCPRange(yMin, yMax));

  for( int i = 0; i < xBin; i++){
    for( int j = 0; j < yBin; j++){
      colorMap->data()->setCell(i, j, NAN);
    }
  }

  if( isChannelMap ){
    QCPAxis * xAxis = colorMap->keyAxis();
    xAxis->ticker()->setTickCount(xbin/tickStep);
    xAxis->ticker()->setTickOrigin(0);
  }

  for( int i = 0; i < 3; i ++){
    for( int j = 0; j < 3; j ++){
      entry[i][j] = 0;
      if( txt[i][j] ) txt[i][j]->setText("0");
    }
  }

  rescaleAxes();
  UpdatePlot();

}

inline  void Histogram2D::RebinY(int ybin, double ymin, double ymax){
  Rebin(xBin-2, xMin, xMax, ybin, ymin, ymax);
}

inline void Histogram2D::Clear(){
  DebugPrint("%s", "Histogram2D");
  for( int i = 0; i < 3; i ++){
    for( int j = 0; j < 3; j ++){
      entry[i][j] = 0;
      txt[i][j]->setText("0");
    }
  }
  colorMap->data()->clear();
  colorMap->data()->setSize(xBin, yBin);
  colorMap->data()->setRange(QCPRange(xMin, xMax), QCPRange(yMin, yMax));
  for( int i = 0; i < xBin; i++){
    for( int j = 0; j < yBin; j++){
      colorMap->data()->setCell(i, j, NAN);
    }
  }

  UpdatePlot();
}


inline void Histogram2D::ClearAllCuts(){
  DebugPrint("%s", "Histogram2D");
  numCut = 0;
  tempCutID = -1;
  lastPlottableID = -1;
  /// Back to front: removeItem/removePlottable renumber everything after the one removed.
  for( int i = cutList.count() - 1; i >= 0 ; i--){
    if( cutList[i].textID < 0 ) continue;
    removeItem(cutList[i].textID);
    removePlottable(cutList[i].plottableID);
  }
  cutList.clear();
  replot();
}

inline void Histogram2D::CommitCut(const QString & name){
  if( tempCut.isEmpty() ) return;

  DrawCut();

  Cut cut;
  cut.poly        = tempCut;
  cut.name        = name;
  cut.entries     = 0;
  cut.id          = tempCutID;
  cut.plottableID = plottableCount() - 1;

  QCPItemText * text = new QCPItemText(this);
  text->setText(name);
  text->position->setCoords(tempCut[0].rx(), tempCut[0].ry());
  text->setColor(colorCycle[ (tempCutID < 0 ? 0 : tempCutID) % colorCycle.count() ].first);
  cut.textID = itemCount() - 1;

  cutList.push_back(cut);
}

inline void Histogram2D::PrintCutEntry() const{
  DebugPrint("%s", "Histogram2D");
  if( numCut == 0 ) return;
  printf("=============== There are %d cuts. (%lld)\n", numCut, cutList.count());
  for( const Cut & cut : cutList ){
    if( cut.poly.isEmpty() ) continue;
    printf("%10s | %d \n", cut.name.toStdString().c_str(), cut.entries);
  }
}

inline void Histogram2D::DrawCut(){
  DebugPrint("%s", "Histogram2D");
  //The histogram is the 1st plottable.
  // the lastPlottableID should be numCut+ 1
  if( lastPlottableID != numCut ){
    removePlottable(lastPlottableID);
  }
  
  if(tempCut.size() > 0) {
      QCPCurve *polygon = new QCPCurve(xAxis, yAxis);
      lastPlottableID = plottableCount() - 1;

      int colorID = tempCutID% colorCycle.count();
      QPen pen(colorCycle[colorID].first);
      pen.setWidth(1);
      polygon->setPen(pen);

      QVector<QCPCurveData> dataPoints;
      for (const QPointF& point : tempCut) {
          dataPoints.append(QCPCurveData(dataPoints.size(), point.x(), point.y()));
      }
      polygon->data()->set(dataPoints, false);
  }
  replot();

  // qDebug() << "Plottable count : " <<  plottableCount() << ", cutList.count :" << cutList.count() << ", cutID :" << lastPlottableID;
}

inline void Histogram2D::rightMouseClickMenu(QMouseEvent * event){
  DebugPrint("%s", "Histogram2D");
  usingMenu = true;
  setSelectionRectMode(QCP::SelectionRectMode::srmNone);

  QMenu *menu = new QMenu(this);
  menu->setAttribute(Qt::WA_DeleteOnClose);

  QAction * a1 = menu->addAction("UnZoom");
  QAction * a6 = menu->addAction("Set/UnSet Log-Z");
  QAction * a2 = menu->addAction("Clear hist.");
  QAction * a3 = menu->addAction("Toggle Stat.");
  QAction * a4 = menu->addAction("Rebin (clear histogram)");
  QAction * a8 = menu->addAction("Load Cut(s)");
  QAction * a5 = menu->addAction("Create a Cut");
  
  QAction * b0 = nullptr;
  QAction * b1 = nullptr;
  QAction * b2 = nullptr;
  if( numCut > 0 ) {
    menu->addSeparator();
    b0 = menu->addAction("Save Cut(s)");
    b2 = menu->addAction("Add/Edit names to Cuts");
    b1 = menu->addAction("Clear all Cuts");
  }
  for( int i = 0; i < cutList.size(); i++){
    if( cutList[i].poly.isEmpty()) continue;
    QAction * delAction = menu->addAction("Delete " + cutList[i].name + " ["+ colorCycle[cutList[i].id%colorCycle.count()].second+"]");
    /// Carry the index on the action. Recovering it by parsing the label back out only ever
    /// worked for the auto-generated "Cut-N" names, and a renamed cut deleted the wrong one.
    delAction->setData(i);
  }

  QAction *selectedAction = menu->exec(event->globalPosition().toPoint());

  // qDebug() << "=======================";
  // qDebug() << selectedAction;
  // qDebug() << b2;

  if( selectedAction == nullptr ){
    usingMenu = false;
    return;
  }

  if( selectedAction == a1 ){
    xAxis->setRangeLower(xMin);
    xAxis->setRangeUpper(xMax);
    yAxis->setRangeLower(yMin);
    yAxis->setRangeUpper(yMax);
    replot();
  }

  if( selectedAction == a2 ) {
    Clear();
  }

  if( selectedAction == a3 ){
    for( int i = 0; i < 3; i ++ ){
      for( int j = 0; j < 3; j ++ ){
        box[i][j]->setVisible( !box[i][j]->visible());
        txt[i][j]->setVisible( !txt[i][j]->visible());
      }
    }
    replot();
  }

  if( selectedAction == a4){
    rightMouseClickRebin();
  }

  if( selectedAction == a5 ){
    tempCut.clear();
    tempCutID ++;
    isDrawCut= true;
    numCut ++;
  }

  if( selectedAction == a6){
    if( !isLogZ ){
      colorMap->setDataScaleType(QCPAxis::stLogarithmic);
      isLogZ = true;
    }else{
      colorMap->setDataScaleType(QCPAxis::stLinear);
      isLogZ = false;
    }
    replot();
  }

  if( selectedAction == a8 ){ // load Cuts
    QString filePath = QFileDialog::getOpenFileName(this, 
                                                "Load Cuts from File", 
                                                settingPath, 
                                                "Text file (*.txt)");

    if (!filePath.isEmpty()) LoadCuts(filePath);      
  }

  //*==================================== when there are cuts
  if( selectedAction == b0 ){ // Save Cuts
    
    QString filePath = QFileDialog::getSaveFileName(this, 
                                                    "Save Cuts to File", 
                                                    settingPath, 
                                                    "Text file (*.txt)");

    if (!filePath.isEmpty()) SaveCuts(filePath);
  }

  if( selectedAction == b1 ){
    ClearAllCuts();
  }

  if( selectedAction == b2 ){

    QDialog dialog(this);
    dialog.setWindowTitle("Add/Edit name of cuts ");

    QFormLayout layout(&dialog);

    for(int i = 0; i < cutList.count(); i++){
      if( cutList[i].textID < 0 ) continue;
      QLineEdit * le = new QLineEdit(&dialog);
      layout.addRow(colorCycle[i%colorCycle.count()].second, le);
      le->setText( cutList[i].name );
      connect(le, &QLineEdit::textChanged, this, [=](){
        le->setStyleSheet("color : blue;");
      });
      connect(le, &QLineEdit::returnPressed, this, [=](){
        le->setStyleSheet("");
        cutList[i].name = le->text();
        ((QCPItemText *) this->item(cutList[i].textID))->setText(le->text());
        replot();
      });
    }
    dialog.exec();
  }

  if( selectedAction && numCut > 0 && selectedAction->text().contains("Delete ") ){

    /// The index rides on the action, see where the menu is built. It used to be recovered by
    /// parsing the label -- which only ever worked for the auto-generated "Cut-N" names, and
    /// silently deleted cut 0 for anything else.
    bool isCutID = false;
    const int cutID = selectedAction->data().toInt(&isCutID);
    if( !isCutID || cutID < 0 || cutID >= cutList.count() ){
      usingMenu = false;
      return;
    }

    if( cutList[cutID].textID >= 0 )      removeItem(cutList[cutID].textID);
    if( cutList[cutID].plottableID >= 0 ) removePlottable(cutList[cutID].plottableID);
    replot();

    numCut --;
    /// Tombstone, do not erase -- the surviving cuts keep their indices, which is what the
    /// delete actions' setData values and the shift below both refer to.
    cutList[cutID] = Cut();
    cutList[cutID].entries = -1;

    /// Everything after the removed item has shifted down by one in QCustomPlot's own lists.
    /// Skip the tombstones: they have nothing left to renumber, and the old code walked them
    /// down past -1 on every subsequent delete.
    for( int i = cutID + 1; i < cutList.count() ; i++){
      if( cutList[i].textID >= 0 )      cutList[i].textID --;
      if( cutList[i].plottableID >= 0 ) cutList[i].plottableID --;
    }

    if( numCut == 0 ){
      tempCutID = -1;
      lastPlottableID = -1;
      cutList.clear();
    }
  }



  usingMenu = false;

}

inline void Histogram2D::rightMouseClickRebin(){
  DebugPrint("%s", "Histogram2D");
  QDialog dialog(this);
  dialog.setWindowTitle("Rebin histogram");

  QFormLayout layout(&dialog);

  QLabel * info = new QLabel(&dialog);
  info->setStyleSheet("color:red;");
  info->setText("This will also clear histogram!!");
  layout.addRow(info);

  QStringList nameListX = {"Num. x-Bin", "x-Min", "x-Max"};
  QLineEdit* lineEditX[3];
  for (int i = 0; i < 3; ++i) {
      lineEditX[i] = new QLineEdit(&dialog);
      layout.addRow(nameListX[i] + " : ", lineEditX[i]);
  }
  lineEditX[0]->setText(QString::number(xBin-2));
  lineEditX[1]->setText(QString::number(xMin));
  lineEditX[2]->setText(QString::number(xMax));

  QStringList nameListY = {"Num. y-Bin", "y-Min", "y-Max"};
  QLineEdit* lineEditY[3];
  for (int i = 0; i < 3; ++i) {
      lineEditY[i] = new QLineEdit(&dialog);
      layout.addRow(nameListY[i] + " : ", lineEditY[i]);
  }
  lineEditY[0]->setText(QString::number(yBin-2));
  lineEditY[1]->setText(QString::number(yMin));
  lineEditY[2]->setText(QString::number(yMax));

  QLabel * msg = new QLabel(&dialog);
  msg->setStyleSheet("color:red;");
  layout.addRow(msg);

  QDialogButtonBox buttonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, &dialog);
  layout.addRow(&buttonBox);

  double number[3][2];

  QObject::connect(&buttonBox, &QDialogButtonBox::accepted, [&]() {
      int OKcount = 0;
      bool conversionOk = true;
      for( int i = 0; i < 3; i++ ){
        number[i][0] = lineEditX[i]->text().toDouble(&conversionOk);
        if( conversionOk ){
          OKcount++;
        }else{
          msg->setText(nameListX[i] + " is invalid.");
          return;
        }
      }
      for( int i = 0; i < 3; i++ ){
        number[i][1] = lineEditY[i]->text().toDouble(&conversionOk);
        if( conversionOk ){
          OKcount++;
        }else{
          msg->setText(nameListY[i] + " is invalid.");
          return;
        }
      }

      if( OKcount == 6 ) {
        if( number[0][0] <= 0 ) {
          msg->setText( nameListX[0] + " is zero or negative" );
          return;
        }
        if( number[0][1] <= 0 ) {
          msg->setText( nameListY[0] + " is zero or negative" );
          return;
        }

        if( number[2][0] > number[1][0] && number[2][1] > number[1][1]  ) {
          dialog.accept();
        }else{
          /// These are the negation of the accept test, not a copy of it. Written as a copy, an
          /// invalid y-range was reported as an x-range fault and both being invalid said nothing
          /// at all -- the dialog just refused to close. Collect them so both axes are reported.
          QString err;
          if( number[2][0] <= number[1][0] ){
            err = nameListX[2] + " is not larger than " + nameListX[1];
          }
          if( number[2][1] <= number[1][1] ){
            if( !err.isEmpty() ) err += "; ";
            err += nameListY[2] + " is not larger than " + nameListY[1];
          }
          msg->setText(err);
        }
      }
  });

  QObject::connect(&buttonBox, &QDialogButtonBox::rejected, [&]() { dialog.reject();});

  if( dialog.exec() == QDialog::Accepted ){
    emit AboutToRebin();
    isBusy = true;
    Rebin((int)number[0][0], number[1][0], number[2][0], (int)number[0][1], number[1][1], number[2][1]);
    isBusy = false;
    emit ReBinned();
  }

}

inline void Histogram2D::SaveCuts(QString cutFileName){

  QFile file(cutFileName);
  if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    QTextStream out(&file);

    // Define the text to write
    QStringList lines;

    /// Deleted cuts are skipped. They used to be written out as a bare "====== " header with no
    /// points, which on reload advances the cut counter and the colour cycle for a cut that is
    /// never created.
    for( const Cut & cut : cutList ){
      if( cut.poly.isEmpty() ) continue;
      lines << "====== "+ cut.name;
      for( const QPointF & pt : cut.poly ){
        lines << QString::number(pt.x(), 'g', 5) + "," +  QString::number(pt.y(), 'g', 5);
      }
    }

    lines << "#===== End of File";

    // Write each line to the file
    for (const QString &line : lines) out << line << "\n";

    // Close the file
    file.close();
    qDebug() << "File written successfully to" << cutFileName;
  }else{
    qWarning() << "Unable to open file" << cutFileName;
  }

}

inline void Histogram2D::LoadCuts(QString cutFileName){

  QFile file(cutFileName);
  QString cutNameTemp;

  // Open the file in read mode
  if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
      QTextStream in(&file);

      ClearAllCuts();
      tempCut.clear();

      // Read each line and append to the QStringList

      while (!in.atEnd()) {
        QString line = in.readLine();

        if( line.contains("======") ){
          /// The header closes the PREVIOUS cut, if there was one.
          CommitCut(cutNameTemp);
          tempCut.clear();
          tempCutID ++;
          numCut ++;

          int spacePos = line.indexOf(' ');
          cutNameTemp = line.mid(spacePos + 1);
          continue;
        }

        if( line.contains("#==") ) {
          /// End-of-file marker closes the last cut. CommitCut() does nothing on an empty
          /// polygon, which is the case for a file whose last "====== name" header is followed
          /// straight by this marker -- tempCut[0] there would be out of bounds.
          CommitCut(cutNameTemp);
          break;
        }else{
          QStringList haha = line.split(",");
          // qDebug() << haha;
          if( haha.size() == 2 ){
            tempCut.push_back(QPointF(haha[0].toFloat(), haha[1].toFloat()));
            DrawCut();
          }
        }

      }

      // Close the file
      file.close();
      qDebug() << "File read successfully from" << cutFileName;
      qDebug() << " Number of cut loaded " << numCut << ", " << cutList.count();

      // PrintCutEntry();
      // DrawCut();
      replot();

  } else {
      qWarning() << "Unable to open file" << cutFileName;
  }

}


#endif