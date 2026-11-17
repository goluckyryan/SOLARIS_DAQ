#ifndef CustomWidgets_H
#define CustomWidgets_H

#include <QWidget>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QWheelEvent>
#include <QString>
#include <cmath>
#include <string>

//^=======================================
class RComboBox : public QComboBox{
  Q_OBJECT
  public : 
    RComboBox(QWidget * parent = nullptr): QComboBox(parent){
      setFocusPolicy(Qt::StrongFocus);
    }
  protected:
    void wheelEvent(QWheelEvent * event) override{ event->ignore(); }
};



//^=======================================
class RSpinBox : public QDoubleSpinBox{
  Q_OBJECT
  public : 
    RSpinBox(QWidget * parent = nullptr, int decimal = 0): QDoubleSpinBox(parent){
      setFocusPolicy(Qt::StrongFocus);
      setDecimals(decimal);
    }

    void SetToolTip(double min = -1){
      if( min == -1 ){
        setToolTip("(" + QString::number(minimum()) + " - " + QString::number(maximum()) + ", " + QString::number(singleStep()) + ")");
      }else{
        setToolTip("(" + QString::number(min) + " - " + QString::number(maximum()) + ", " + QString::number(singleStep()) + ")");
      }
      setToolTipDuration(-1);
    }

    /// Decimals from a parameter's step string: "0.001" -> 3, "0.5" -> 1, "8" -> 0. Call before
    /// setting the range, which QDoubleSpinBox rounds to the current decimals.
    void SetDecimalsFromStep(const std::string & step){
      int d = 0;
      const size_t dot = step.find('.');
      if( dot != std::string::npos ){
        d = (int) (step.size() - dot - 1);
        while( d > 0 && step[dot + d] == '0' ) d--;
      }
      setDecimals(d);
    }

    /// The value as the board should receive it: a decimal box keeps its fraction (1.234 stays
    /// 1.234, never 1), an integer box is rounded.
    std::string ValueString() const {
      if( decimals() > 0 ) return QString::number(value(), 'f', decimals()).toStdString();
      return std::to_string((long long) std::llround(value()));
    }

    void SetStyleNormal() { setStyleSheet(""); }
    void SetStyleGreen()  { setStyleSheet("color:green"); }

  signals:
    void returnPressed();
  protected:
    void wheelEvent(QWheelEvent * event) override{ event->ignore(); }

    void keyPressEvent(QKeyEvent * event) override{
      if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        emit returnPressed();
      } else {
        QDoubleSpinBox::keyPressEvent(event);
      }
    }
};

#endif