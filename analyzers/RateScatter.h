#pragma once
//^---- Rate vs time scatter view that lives INSIDE an analysis .so: no ABI surface, no host
//^     symbols, and deliberately no Q_OBJECT -- paintEvent is virtual, so a moc-free subclass
//^     works, while signals/slots would drag moc into the plugin Makefile. The analysis drives
//^     it externally through Sample() from a QTimer it parents to this widget, so the timer
//^     dies with the work area. tests/rateScatter.cpp includes this header directly.
//^
//^ Data model: one series per tag ("all" = total rate, then one per live 2D cut), {t, Hz}
//^ points in a deque retained to the widest zoom. Mouse wheel rescales the visible time
//^ window (5 s .. 1 h), double-click returns to the 600 s default; the view always follows the
//^ newest point. A tag missing from the newest snapshot is STALE (deleted cut): its history
//^ stays drawn, its legend dims, no new points.

#include <QColor>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>
#include <QWidget>

#include <algorithm>
#include <deque>
#include <map>
#include <string>
#include <utility>
#include <vector>

class RateScatter : public QWidget {
public:
  explicit RateScatter(QWidget * parent = nullptr) : QWidget(parent) {}

  static constexpr double kDefaultS = 600.0;   ///< x window at start / after double-click
  static constexpr double kMinS     =   5.0;   ///< deepest wheel zoom in
  static constexpr double kMaxS     = 3600.0;  ///< widest zoom == history kept in the deques

  double windowS() const { return winS_; }
  void   SetWindowS(double s){ winS_ = std::clamp(s, kMinS, kMaxS); update(); }

  struct Series {
    std::deque<std::pair<double,double>> pts;  ///< {t - t0, Hz}
    int  color = 0;                            ///< palette index, fixed at creation
    bool stale = false;                        ///< absent from the latest snapshot
    double last() const { return pts.empty() ? 0.0 : pts.back().second; }
  };

  /// Append one {tag, Hz} snapshot at absolute time nowS [s, caller's monotonic clock].
  /// An EMPTY snapshot is a run boundary -- the host clears the published ledger on stop --
  /// so history is wiped here and the next non-empty snapshot starts a fresh time base.
  void Sample( double nowS, const std::vector<std::pair<std::string,double>> & rows ){
    if( rows.empty() ){ ser_.clear(); ord_.clear(); t0_ = -1.0; yMax_ = 1.0; update(); return; }
    if( t0_ < 0 ) t0_ = nowS;                  ///< first point after a boundary
    const double t = nowS - t0_;
    for( auto & kv : ser_ ) kv.second.stale = true;   ///< live tags are marked back below
    for( const auto & r : rows ){
      auto it = ser_.find( r.first );
      if( it == ser_.end() ){
        Series s;
        s.color = (int)ser_.size();            ///< creation order = palette order
        it = ser_.emplace( r.first, std::move(s) ).first;
        ord_.push_back( r.first );
      }
      Series & s = it->second;
      s.stale = false;
      s.pts.push_back( { t, r.second } );
      /// Retain to kMaxS (the widest zoom), NOT to the live window: zooming back out must be
      /// able to recover history the user zoomed away from mid-run.
      while( !s.pts.empty() && s.pts.front().first < t - kMaxS ) s.pts.pop_front();
    }
    yMax_ = 1.0;
    for( const auto & kv : ser_ )
      for( const auto & q : kv.second.pts ) yMax_ = std::max( yMax_, q.second );
    yMax_ *= 1.15;
    update();
  }

  double t0()   const { return t0_; }           ///< <0 = idle (no run's points)
  double yMax() const { return yMax_; }
  const std::map<std::string,Series> & series() const { return ser_; }

protected:
  void paintEvent( QPaintEvent * ) override {
    QPainter p(this);

    const int L = 56, R = 10, T = 8, B = 24;
    QRectF ax( L, T, std::max(1, width()-L-R), std::max(1, height()-T-B) );

    double tMax = 0;
    for( const auto & kv : ser_ )
      if( !kv.second.pts.empty() ) tMax = std::max(tMax, kv.second.pts.back().first);
    const double xLo = std::max(0.0, tMax - winS_);
    const double xHi = xLo + winS_;

    p.setPen(QColor(110,110,110));
    p.drawRect(ax);
    p.drawLine(QPointF(ax.left(),  ax.center().y()), QPointF(ax.right(), ax.center().y()));
    p.drawText(QRectF(0, ax.top()-9,        L-6, 16), Qt::AlignRight|Qt::AlignVCenter,
               QString::number((long long)(yMax_+0.5)));
    p.drawText(QRectF(0, ax.center().y()-8, L-6, 16), Qt::AlignRight|Qt::AlignVCenter,
               QString::number((long long)(yMax_*0.5+0.5)));
    p.drawText(QRectF(0, ax.bottom()-5,     L-6, 16), Qt::AlignRight|Qt::AlignVCenter, "0");
    p.drawText(QRectF(ax.left()-8,  ax.bottom()+4, 80, 16), Qt::AlignLeft,
               QString::number((long long)xLo) + " s");
    p.drawText(QRectF(ax.right()-72, ax.bottom()+4, 80, 16), Qt::AlignRight,
               QString::number((long long)xHi) + " s");
    p.drawText(QRectF(ax.right()-150, ax.top()-2, 148, 16), Qt::AlignRight,
               QString("last %1 s").arg((long long)winS_));

    for( const auto & tag : ord_ ){
      const Series & s = ser_.at(tag);
      QColor c = palette(s.color);
      if( s.stale ) c = c.darker(190);                ///< deleted cut: keep history, dim it
      p.setPen(Qt::NoPen);
      p.setBrush(c);
      for( const auto & q : s.pts ){
        if( q.first < xLo ) continue;
        const double x = ax.left() + (q.first - xLo) / winS_ * ax.width();
        const double y = ax.bottom() - q.second / yMax_ * ax.height();
        p.drawEllipse(QPointF(x, y), 1.6, 1.6);
      }
    }

    int ly = (int)ax.top() + 4;
    for( const auto & tag : ord_ ){
      const Series & s = ser_.at(tag);
      p.setPen(Qt::NoPen);
      p.setBrush(s.stale ? QColor(170,170,170) : palette(s.color));
      p.drawRoundedRect(QRectF(ax.left()+6, ly+2, 10, 10), 2, 2);
      p.setPen(QColor(30,30,30));
      p.drawText(QRectF(ax.left()+22, ly, ax.width()-30, 15), Qt::AlignLeft|Qt::AlignVCenter,
                 QString("%1   %2 Hz").arg(QString::fromStdString(tag))
                                       .arg(s.last(), 0, 'f', 1));
      ly += 17;
      if( ly > ax.bottom() - 18 ) break;              ///< legend caps at the pane height
    }
  }

  //^---- zoom: wheel rescales the time window (up = in = shorter), double-click returns to the
  //^     default. No panning: a live rate plot always follows the newest point, so anchoring a
  //^     zoom to the cursor would only be honest once pan existed.
  void wheelEvent( QWheelEvent * ev ) override {
    SetWindowS( winS_ * ( ev->angleDelta().y() > 0 ? 0.8 : 1.25 ) );
    ev->accept();
  }
  void mouseDoubleClickEvent( QMouseEvent * ev ) override {
    SetWindowS(kDefaultS);
    ev->accept();
  }

private:
  static QColor palette(int i){
    static const QRgb rgb[] = { 0x000000, 0x1f77b4, 0xd62728, 0x2ca02c,
                                0x9467bd, 0xff7f0e, 0x17becf, 0x666666 };
    return QColor( rgb[i % (int)(sizeof(rgb)/sizeof(rgb[0]))] );
  }

  std::map<std::string,Series> ser_;
  std::vector<std::string>     ord_;     ///< legend order = tag creation order
  double t0_   = -1.0;
  double yMax_ =  1.0;
  double winS_ = kDefaultS;
};
