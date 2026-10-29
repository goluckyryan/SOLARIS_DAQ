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
//^ GUI thread only. It rides the replot/monitor clock (Sample is called from
//^ SingleSpectra::SampleRates), so the window resolution is the sampling period; the counts
//^ themselves stay exact because they come from the fill worker's atomics.
//^
//^ Each key also carries display META (human label + owning digitizer/channel) so consumers --
//^ RateMonitor's picker today, an InfluxDB writer later -- can enumerate and name channels
//^ without parsing keys. The retention window WIDENS on demand: a consumer asking for a longer
//^ interval (a 5 min time base) raises the pruning horizon, so nobody ever gets a window that
//^ silently truncates another consumer's shorter one.
//^====================================================
class RateStore{
public:
  void Sample(const QString & key, qint64 tMs, unsigned long cumTotal, int keepMs){
    if( !chans.contains(key) ) order.append(key);
    Chan & c = chans[key];
    c.keepMs = qMax(c.keepMs, keepMs);                  // the producer's own horizon (label window)
    if( !c.h.empty() && cumTotal < c.h.back().second ) c.h.clear(); // source was cleared
    c.h.append(qMakePair(tMs, cumTotal));
    /// prune everything strictly older than the keep-window, but always retain one
    /// bracket point behind it so a window query can still find its left edge.
    while( c.h.size() > 2 && tMs - c.h[1].first > c.keepMs ) c.h.removeFirst();
  }

  /// Average rate over the trailing intervalMs, from the oldest sample still inside the window.
  /// Returns false while the channel has fewer than two points (or zero elapsed time).
  /// Asking for a window also RETAINS it: the pruning horizon grows to cover this consumer.
  bool Rate(const QString & key, int intervalMs, double & rate){
    auto it = chans.find(key);
    if( it == chans.end() ) return false;
    Chan & c = it.value();
    c.keepMs = qMax(c.keepMs, intervalMs);
    if( c.h.size() < 2 ) return false;
    const QVector<QPair<qint64,unsigned long>> & h = c.h;
    const qint64 tNow = h.back().first;
    int i = h.size() - 1;
    while( i > 0 && tNow - h[i-1].first <= intervalMs ) i--;
    const qint64 dt = tNow - h[i].first;
    if( dt <= 0 ) return false;
    rate = double(h.back().second - h[i].second) * 1000.0 / double(dt);
    return true;
  }

  void Drop(const QString & key){ order.removeAll(key); chans.remove(key); }
  bool Exists(const QString & key) const { return chans.contains(key); }

  /// Idempotent creation. Keys mirror EXISTENCE (a histogram channel, a drawn gate), not data:
  /// the owner registers a key when it appears -- while the DAQ is idle -- and Sample() only
  /// ever adds history. Registration alone promises nothing: Rate() says false until real
  /// samples exist, and picker filters decide on LIVENESS separately (see the meta owner).
  void Register(const QString & key){
    if( chans.contains(key) ) return;
    order.append(key);
    chans.insert(key, Chan());
  }

  /// Wipe every sample history but keep keys and meta: called when a new DAQ run zeroes the
  /// sampling clock, so no window ever straddles the time discontinuity. The next Sample()
  /// per key re-seeds from there.
  void ResetHistory(){
    for( auto it = chans.begin(); it != chans.end(); ++it ) it.value().h.clear();
  }

  /// Registration order, so a consumer's picker list does not reshuffle between opens.
  const QStringList & Keys() const { return order; }

  void SetMeta(const QString & key, int d, int ch, const QString & label){
    // Decorate-only: Sample() CREATES keys. A SetMeta-first would seed Keys() with a phantom
    // that has no history and no owner action to drop it.
    auto it = chans.find(key);
    if( it == chans.end() ) return;
    Chan & c = it.value();
    c.d = d; c.ch = ch;
    if( !label.isEmpty() ) c.label = label;
  }
  QString Label(const QString & key) const   { auto it = chans.constFind(key); return it == chans.constEnd() ? QString() : it->label; }
  int  OwnerD (const QString & key) const    { auto it = chans.constFind(key); return it == chans.constEnd() ? -1 : it->d; }
  int  OwnerCh(const QString & key) const    { auto it = chans.constFind(key); return it == chans.constEnd() ? -1 : it->ch; }

private:
  struct Chan {
    QVector<QPair<qint64,unsigned long>> h;
    int keepMs = 0;
    QString label;
    int d = -1, ch = -1;
  };
  QHash<QString, Chan> chans;
  QStringList order;
};

#endif
