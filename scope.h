#ifndef Scope_H
#define Scope_H


#include <QMainWindow>
#include <QChart>
#include <QChartView>
#include <QSpinBox>
#include <QPushButton>
#include <QCheckBox>
#include <QLineEdit>
#include <QComboBox>
#include <QGridLayout>
#include <QLineSeries>
#include <QRubberBand>
#include <QMouseEvent>
#include <QGestureEvent>

#include "macro.h"
#include "ClassDigitizer2Gen.h"
#include "manyThread.h"
#include "CustomWidgets.h"

class Trace : public QChart{
public:
  explicit Trace(QGraphicsItem *parent = nullptr, Qt::WindowFlags wFlags = {})
    : QChart(QChart::ChartTypeCartesian, parent, wFlags){
    grabGesture(Qt::PanGesture);
    grabGesture(Qt::PinchGesture);
  }
  ~Trace(){}

protected:
  bool sceneEvent(QEvent *event){
    if (event->type() == QEvent::Gesture) return gestureEvent(static_cast<QGestureEvent *>(event));
    return QChart::event(event);
  }

private:
  bool gestureEvent(QGestureEvent *event){
    if (QGesture *gesture = event->gesture(Qt::PanGesture)) {
      QPanGesture *pan = static_cast<QPanGesture *>(gesture);
      QChart::scroll(-(pan->delta().x()), pan->delta().y());
    }

    if (QGesture *gesture = event->gesture(Qt::PinchGesture)) {
      QPinchGesture *pinch = static_cast<QPinchGesture *>(gesture);
      if (pinch->changeFlags() & QPinchGesture::ScaleFactorChanged) QChart::zoom(pinch->scaleFactor());
    }
    return true;
  }

private:

};

class TraceView : public QChartView{
public:
  TraceView(QChart * chart, QWidget * parent = nullptr): QChartView(chart, parent){
    m_isTouching = false;
    this->setRubberBand(QChartView::RectangleRubberBand);
  }

protected:
  bool viewportEvent(QEvent *event){
    if (event->type() == QEvent::TouchBegin) {
      m_isTouching = true;
      chart()->setAnimationOptions(QChart::NoAnimation);
    }
    return QChartView::viewportEvent(event);
  }
  void mousePressEvent(QMouseEvent *event){
    if (m_isTouching) return;
    QChartView::mousePressEvent(event);
  }
  void mouseMoveEvent(QMouseEvent *event){
    if (m_isTouching) return;
    QChartView::mouseMoveEvent(event);
  }
  void mouseReleaseEvent(QMouseEvent *event){
    if (m_isTouching)  m_isTouching = false;
    chart()->setAnimationOptions(QChart::SeriesAnimations);
    QChartView::mouseReleaseEvent(event);
  }
  void keyPressEvent(QKeyEvent *event){
    switch (event->key()) {
      case Qt::Key_Plus:  chart()->zoomIn(); break;
      case Qt::Key_Minus: chart()->zoomOut(); break;
      case Qt::Key_Left:  chart()->scroll(-10, 0); break;
      case Qt::Key_Right: chart()->scroll(10, 0); break;
      case Qt::Key_Up: chart()->scroll(0, 10); break;
      case Qt::Key_Down: chart()->scroll(0, -10);  break;
      case Qt::Key_R : chart()->axes(Qt::Vertical).first()->setRange(-16384, 65536); break;
      default: QGraphicsView::keyPressEvent(event); break;
    }
  }

private:
  bool m_isTouching;
};

//^=======================================
class Scope : public QMainWindow{
  Q_OBJECT

public:
  Scope(Digitizer2Gen ** digi, unsigned int nDigi, ReadDataThread ** readDataThread, QMainWindow * parent = nullptr);
  ~Scope();

public slots:
  void ReadScopeSettings(); // read from digitizer and show;
  void UpdateSettingsFromMemeory();

private slots:
  void StartScope();
  void StopScope();
  void UpdateScope();
  void ScopeControlOnOff(bool on);
  void ScopeReadSpinBoxValue(int iDigi, int ch, RSpinBox *sb, const Reg digPara);
  void ScopeReadComboBoxValue(int iDigi, int ch, RComboBox *cb, const Reg digPara);
  void ScopeMakeSpinBox(RSpinBox * &sb, QString str,  QGridLayout* layout, int row, int col, const Reg digPara);
  void ScopeMakeComoBox(RComboBox * &cb, QString str, QGridLayout* layout, int row, int col, const Reg digPara);
  void ProbeChange(RComboBox * cb[], const int size);

  void closeEvent(QCloseEvent * event){
    StopScope();  
    emit CloseWindow();
    event->accept();
  }

signals:

  void CloseWindow();
  void UpdateScalar();
  void SendLogMsg(const QString &msg);
  void UpdateOtherPanels();
  void TellSettingsPanelControlOnOff();
  void TellACQOnOff(const bool onOff);

private:

  Digitizer2Gen ** digi;
  unsigned short nDigi;

  ReadDataThread ** readDataThread;   
  UpdateTraceThread * updateTraceThread;

  QChart      * plot;
  QLineSeries * dataTrace[6];
  RComboBox   * cbScopeDigi;
  RComboBox   * cbScopeCh;
  QPushButton * bnScopeReset;
  QPushButton * bnScopeReadSettings;
  
  QCheckBox * chkSetAllChannel;
  QPushButton * bnScopeStart;
  QPushButton * bnScopeStop;
  
  RComboBox   * cbAnaProbe[2];
  RComboBox   * cbDigProbe[4];
  RSpinBox * sbRL; // record length
  RSpinBox * sbPT; // pre trigger
  RSpinBox * sbDCOffset;
  RSpinBox * sbThreshold;
  RSpinBox * sbTimeRiseTime;
  RSpinBox * sbTimeGuard;
  RSpinBox * sbTrapRiseTime;
  RSpinBox * sbTrapFlatTop;
  RSpinBox * sbTrapPoleZero;
  RSpinBox * sbEnergyFineGain;
  RSpinBox * sbTrapPeaking;
  RComboBox * cbPolarity;
  RComboBox * cbWaveRes;
  RComboBox * cbTrapPeakAvg;

  QLineEdit * leTriggerRate;

  RSpinBox * sbBaselineGuard;
  RSpinBox * sbPileUpGuard;
  RComboBox * cbBaselineAvg;
  RComboBox * cbLowFreqFilter;

  bool allowChange;

};

#endif