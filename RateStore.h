#ifndef RATE_STORE_H
#define RATE_STORE_H

#include <QHash>
#include <QVector>
#include <QString>

//^====================================================
//^ A tiny sliding-window dN/dt ledger, owned by SingleSpectra.
//^
//^ Producers (the publish path, later the gate/monitor plumbing) call Sample() with a
//^ MONOTONICALLY INCREASING cumulative count per named channel -- a histogram's in-range
//^ total, or the bin-sum inside a 1D gate. The store keeps the recent (time, count) history
//^ and Rate() answers the average dN/dt over the trailing intervalMs window, using the oldest
//^ sample still inside it (so a window longer than the run so far degrades gracefully to the
//^ lifetime average rather than lying).
//^
//^ A Sample() whose count DECREASED relative to the previous one means the source was
//^ cleared (Clear / Rebin / Clear All Hist.): the history is dropped and the new point seeds
//^ a fresh baseline, so the first rate after a clear is again computed from real deltas.
//^
//^ GUI thread only. It rides the replot clock (Sample is called from PublishHist1D), so the
//^ window resolution is the publish period; the counts themselves stay exact because they
//^ come from the fill worker's atomics.
//^====================================================
class RateStore{
public:
  void Sample(const QString & key, qint64 tMs, unsigned long cumTotal, int keepMs){
    Chan & c = chans[key];
    if( !c.h.empty() && cumTotal < c.h.back().second ) c.h.clear(); // source was cleared
    c.h.append(qMakePair(tMs, cumTotal));
    /// prune everything strictly older than the consumer's keep-window, but always retain one
    /// bracket point behind it so a window query can still find its left edge.
    while( c.h.size() > 2 && tMs - c.h[1].first > keepMs ) c.h.removeFirst();
  }

  /// Average rate over the trailing intervalMs, from the oldest sample still inside the window.
  /// Returns false while the channel has fewer than two points (or zero elapsed time).
  bool Rate(const QString & key, int intervalMs, double & rate) const{
    auto it = chans.constFind(key);
    if( it == chans.constEnd() || it->h.size() < 2 ) return false;
    const QVector<QPair<qint64,unsigned long>> & h = it->h;
    const qint64 tNow = h.back().first;
    int i = h.size() - 1;
    while( i > 0 && tNow - h[i-1].first <= intervalMs ) i--;
    const qint64 dt = tNow - h[i].first;
    if( dt <= 0 ) return false;
    rate = double(h.back().second - h[i].second) * 1000.0 / double(dt);
    return true;
  }

  void Drop(const QString & key){ chans.remove(key); }

private:
  struct Chan { QVector<QPair<qint64,unsigned long>> h; };
  QHash<QString, Chan> chans;
};

#endif
