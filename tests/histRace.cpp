//^====================================================
//^ Headless race harness for the fill/repaint decoupling.
//^
//^ Drives the REAL MainWindow on/offscreen: OpenDigitizers -> Fill Histograms -> AutoRun
//^ (board ACQ + ReadDataThread + both timers), then while data is streaming hammers the
//^ GUI-thread paths that reallocate the buffers the worker writes into:
//^   - pane / split combo changes  -> fenced SetPaneSource + reparenting
//^   - Rebin through AboutToRebin/ReBinned -> the exact menu sequence, fenced
//^   - Clear, chkAutoRefresh + sbRefresh -> replotTimer restarted live
//^ and samples GetTotal1D progress to prove events actually flowed.
//^
//^ Exit codes: 0 = clean, 2 = board not connected / no ACQ, 3 = no events flowed.
//^ Build: see tests/build_race.sh ; run under gdb or ASAN to catch crashes with stacks.
//^====================================================
#include <QApplication>
#include <QTimer>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QElapsedTimer>
#include <QDebug>
#include <QRandomGenerator>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include <thread>
#include <atomic>

#include "mainwindow.h"
#include "SingleSpectra.h"
#include "Histogram1D.h"
#include "Histogram2D.h"

static int   nRebin   = 0;
static int   nClear   = 0;
static int   nCombo   = 0;
static int   nRefresh = 0;
static int   nGate    = 0;
static int   nKeyError = 0;
static int   nBoundsError = 0;
static int   nSumError = 0;
static int   nSaveErr = 0;
static int   nGateWithCounts = 0;
static bool  acqStarted = false;
static bool  sawEvents  = false;

//^---- phase 2: the real board at 192.168.0.100 streams nothing without programmed settings, so
//^ the worker hot path is also hammered with a dummy board fed by one producer thread per ring
//^ (RingBuffer is single-producer) at full tilt, while Rebin/pane-churn realloc the count arrays
//^ underneath it. This is the interleave the suspendFilling fence exists to protect.
static std::atomic<bool> feedStop{false};
static std::atomic<unsigned long> fedCount{0};
static Digitizer2Gen   * dummy = nullptr;
static SingleSpectra * ss2     = nullptr;

static int pick( int n ){ return QRandomGenerator::global()->bounded(n); }

static SingleSpectra * curSS( MainWindow * w ){
  if( ss2 ) return ss2;
  return w ? w->findChild<SingleSpectra *>() : nullptr;
}

static void EnableFill( SingleSpectra * ss ){
  for( QCheckBox * cb : ss->findChildren<QCheckBox *>() )
    if( cb->text() == "Fill Histograms" && !cb->isChecked() ) cb->setChecked(true);
}

static void ChaosStep( MainWindow * w, SingleSpectra * ss ){
  switch( pick(12) ){
    case 0: case 1: case 2: {  // combo changes: pane sources and the split selector
      QList<QComboBox *> cbs = ss->findChildren<QComboBox *>();
      if( cbs.isEmpty() ) break;
      QComboBox * cb = cbs[ pick(cbs.size()) ];
      int n = cb->count();
      if( n < 2 ) break;
      cb->setCurrentIndex( (cb->currentIndex() + 1 + pick(n-1)) % n );
      nCombo++;
      break;
    }
    case 3: case 4: case 5: {  // Rebin 1D through the exact menu sequence
      QList<Histogram1D *> hs = ss->findChildren<Histogram1D *>();
      if( hs.isEmpty() ) break;
      Histogram1D * h = hs[ pick(hs.size()) ];
      static const int    bins[3] = {   512,  1024,  4096 };
      static const double maxs[3] = { 30000., 60000., 1000. };
      const int k = pick(3);
      QMetaObject::invokeMethod( h, "AboutToRebin" );
      h->Rebin( bins[k], 0., maxs[k] );
      QMetaObject::invokeMethod( h, "ReBinned" );
      h->UpdatePlot();
      nRebin++;
      break;
    }
    case 6: {                   // Rebin the 2D overviews (colormap realloc)
      QList<Histogram2D *> hs2 = ss->findChildren<Histogram2D *>();
      if( hs2.isEmpty() ) break;
      Histogram2D * h = hs2[ pick(hs2.size()) ];
      QMetaObject::invokeMethod( h, "AboutToRebin" );
      h->Rebin( 64, 0, 64, 1024, 0., 60000. );
      QMetaObject::invokeMethod( h, "ReBinned" );
      nRebin++;
      break;
    }
    case 7: {                   // Clear + republish mid-flight
      QList<Histogram1D *> hs = ss->findChildren<Histogram1D *>();
      if( hs.isEmpty() ) break;
      hs[ pick(hs.size()) ]->Clear();
      nClear++;
      break;
    }
    case 8: case 9: case 10: {  // gate churn: create/destroy items + store keys under live publish,
                                // interval re-windows the same store, GateSum reads the bins just pushed
      QList<Histogram1D *> hs = ss->findChildren<Histogram1D *>();
      if( hs.isEmpty() ) break;
      Histogram1D * h = nullptr;   // only observe gates on a hist that actually has published counts
      for( int t = 0; t < 16 && !h; t++ ){
        Histogram1D * cand = hs[ pick(hs.size()) ];
        if( cand->GetTotalEntry() > 0 ) h = cand;
      }
      if( !h ) break;
      const int kAdd = h->AddGate();
      if( kAdd >= 0 ){
        const QString nm = h->GateName(kAdd);
        h->RenameGate(kAdd, "X"+nm);
        if( h->FindGate(nm)    >= 0 ) printf("STALE OLD NAME on %p\n",(void*)h);
        if( h->FindGate("X"+nm) < 0 ) printf("LOST NEW NAME on %p\n",(void*)h);
        h->RemoveGate( QString("X"+nm) );           // delete-by-name path
      }

      // the trap: delete a MIDDLE gate; every survivor must keep its own RateStore key
      if( h->GateCount() >= 2 ){
        const QString k1 = h->GateKey(1);
        h->RemoveGate(0);
        if( h->GateKey(0) != k1 ){ printf("KEY SHIFT AFTER MID DELETE on %p\n",(void*)h); nKeyError++; }
      }
      if( h->GateCount() > 2 ) h->RemoveGate(1);    // arbitrary middle delete
      else if( h->GateCount() > 1 ) h->RemoveGate(0);
      if( h->GateCount() == 0 ) h->AddGate();        // always leave one alive for the sums below

      static const int win[6] = { 1000, 1000, 2000, 5000, 10000, 30000 };
      h->SetRateIntervalMs( win[pick(6)] );

      // SetGateBounds: INVERTED pair, pushed far out of range, must come back full-range ordered
      // and clamped -- and a full-range gate must then count exactly the in-range total.
      {
        const int kb = h->AddGate();
        if( kb >= 0 ){
          const double xlo = h->GetXMin(), xhi = h->GetXMax();
          h->SetGateBounds(kb, xhi + 1.0e6, xlo - 1.0e6);   // inverted AND out of range both ways
          if( h->GateLow(kb) != xlo || h->GateHigh(kb) != xhi ){
            printf("BOUNDS NOT CLAMPED/SWAPPED on %p (%g..%g vs %g..%g)\n",
                   (void*)h, h->GateLow(kb), h->GateHigh(kb), xlo, xhi);
            nBoundsError++;
          }
          // The publish snapshots total BEFORE the bins, so both orders are legitimately
          // off by the entries arriving in that microsecond gap; anything larger is a bug.
          const unsigned long gs = h->GateSum(kb), gt = h->GetTotalEntry();
          const long d = (long)(gs > gt ? gs - gt : gt - gs);
          if( d > 1000 ){
            printf("FULL-RANGE GATE SUM %lu vs TOTAL %lu on %p\n", gs, gt, (void*)h);
            nSumError++;
          }
          h->RemoveGate(kb);
        }
        // default-positioned gate must be centered on the axis, 10 % wide
        const int kc = h->AddGate();
        if( kc >= 0 ){
          const double c = 0.5*(h->GateLow(kc) + h->GateHigh(kc));
          const double w = h->GateHigh(kc) - h->GateLow(kc);
          const double xc = 0.5*(h->GetXMin() + h->GetXMax());
          const double xw = 0.10*(h->GetXMax() - h->GetXMin());
          if( qAbs(c-xc) > 1e-6 || qAbs(w-xw) > 1e-6 ){
            printf("GATE NOT CENTERED/WIDE c=%g w=%g vs %g/%g\n", c, w, xc, xw);
            nBoundsError++;
          }
          h->RemoveGate(kc);
        }
      }

      for( int k = 0; k < h->GateCount(); k++ ){
        if( h->GateSum(k) > 0 ) nGateWithCounts++;
        if( h->GateHigh(k) < h->GateLow(k) ){ printf("GATE INVERSION on %p\n",(void*)h); }
      }
      nGate++;
      break;
    }
    default: {                  // live re-interval of the replot timer itself
      QList<QCheckBox *> cbs = ss->findChildren<QCheckBox *>();
      for( QCheckBox * cb : cbs )
        if( cb->text().startsWith("Auto Refresh") ) cb->setChecked( !cb->isChecked() );
      QList<QDoubleSpinBox *> sbs = ss->findChildren<QDoubleSpinBox *>();
      for( QDoubleSpinBox * sb : sbs ){
        static const double vals[5] = { 50., 100., 250., 500., 1000. };
        sb->setValue( vals[pick(5)] );
      }
      nRefresh++;
      break;
    }
  }
  (void)w;
}

int main( int argc, char * argv[] ){
  setvbuf( stdout, nullptr, _IONBF, 0 );
  qInstallMessageHandler( [](QtMsgType, const QMessageLogContext &, const QString & m){ fprintf(stderr, "%s\n", qPrintable(m)); } );
  QApplication a(argc, argv);

  MainWindow * w = new MainWindow();
  w->show();

  const int runSec  = (argc > 1) ? QString(argv[1]).toInt() : 20;
  const int feedSec = (argc > 2) ? QString(argv[2]).toInt() : 12;

  // phase 2 state: dummy board + one feeder thread per ring + its own chaos budget
  int p2elapsed = 0;
  std::vector<std::thread> feeds;

  QTimer phase, chaos;
  phase.setInterval(2000);
  QObject::connect( &phase, &QTimer::timeout, [&](){
    // OpenDigitizers() calls processEvents() internally; a nested tick re-entering it would
    // open a second CAEN session on the same board and stall. The real app cannot re-enter
    // here (the buttons disable for the duration); guard the harness the same way.
    static bool inTick = false;
    if( inTick ) return;
    inTick = true;
    struct Guard { ~Guard(){ inTick = false; } } guard;

    static int elapsed = 0;
    elapsed += 2;

    //--------------- phase 2 : synthetic hot path
    if( dummy ){
      p2elapsed += 2;
      if( ss2 ){
        unsigned long tot = 0;
        for( int c = 0; c < 8; c++ ) tot += ss2->GetTotal1D(0, c);
        printf("p2 t=%3ds fed=%lu ringTotals(ch0..7)=%lu rebin=%d clear=%d combo=%d refresh=%d gate=%d gatePos=%d\n",
               p2elapsed, fedCount.load(), tot, nRebin, nClear, nCombo, nRefresh, nGate, nGateWithCounts);
      }
      if( p2elapsed < feedSec ) return;

      //--- settings round-trip: named/moved gates + rate window must survive save -> mutate -> load
      {
        QList<Histogram1D *> hs;
        if( ss2 ) hs = ss2->findChildren<Histogram1D *>();
        Histogram1D * h = nullptr;
        for( Histogram1D * c : hs ) if( c->GetTotalEntry() > 0 ){ h = c; break; }
        if( h ){
          const double x0 = h->GetXMin(), w = h->GetXMax() - h->GetXMin();
          h->ClearGates();
          h->SetRateIntervalMs(5000);
          const int a = h->AddGate(); h->RenameGate(a, "physA"); h->SetGateBounds(a, x0+0.20*w, x0+0.30*w);
          const int b = h->AddGate(); h->RenameGate(b, "physB"); h->SetGateBounds(b, x0+0.60*w, x0+0.62*w);
          const int idB = h->GateId(b);
          h->RemoveGate(a);                       // middle delete: B keeps its id/color
          ss2->SaveSetting();
          h->ClearGates(); h->SetRateIntervalMs(1000);   // destroy local state
          ss2->LoadSetting();
          const int kb = h->FindGate("physB");
          if( kb < 0 ){ printf("ROUNDTRIP: physB missing\n"); nSaveErr++; }
          else{
            if( h->GateId(kb) != idB ){ printf("ROUNDTRIP: id %d != %d\n", h->GateId(kb), idB); nSaveErr++; }
            if( qAbs(h->GateLow(kb) - (x0+0.60*w)) > 1e-6*w || qAbs(h->GateHigh(kb) - (x0+0.62*w)) > 1e-6*w ){
              printf("ROUNDTRIP: bounds lost (%g..%g)\n", h->GateLow(kb), h->GateHigh(kb)); nSaveErr++; }
          }
          if( h->RateIntervalMs() != 5000 ){ printf("ROUNDTRIP: rateMs=%d\n", h->RateIntervalMs()); nSaveErr++; }
        } else printf("ROUNDTRIP: no published hist found\n");

        //--- migration: seed a legacy txt, remove the json, LoadSetting must apply + migrate
        {
          const QString jsonP = "/home/ryan/ExpData/solaris_test/testing/HistogramSettings.json";
          const QString txtP  = "/home/ryan/ExpData/solaris_test/testing/HistogramSettings.txt";
          QFile::remove(txtP); QFile::remove(txtP + ".bak");
          {
            QFile t(txtP);
            if( t.open(QIODevice::Text|QIODevice::WriteOnly) ){
              t.write("======= 999\n");
              t.write(" 0 321    10  4321\n");
              for( int c = 1; c < 64; c++ ) t.write( QString(" %1 %2 %3 %4\n").arg(c,2).arg(200).arg(0).arg(5000).toUtf8() );
              t.write("64 64 0 64 200 0 5000\n");
              t.write("####### PANEL\n");
              t.write("ROWS_COLS 1 2\n");
              t.write("REFRESH   0 500\n");
              t.write("PANE      0 999 1\n");
              t.write("PANE      1 999 2\n");
              t.write("##========== End of file\n");
              t.close();
            }
            QFile::remove(jsonP);
          }
          if( ss2 ) ss2->LoadSetting();
          const bool jsonBack = QFile::exists(jsonP);
          const bool txtGone  = !QFile::exists(txtP) && QFile::exists(txtP + ".bak");
          bool rebinOK = false, splitOK = false;
          if( jsonBack ){
            QFile jf(jsonP);
            if( jf.open(QIODevice::ReadOnly) ){
              const QJsonObject root = QJsonDocument::fromJson(jf.readAll()).object();
              rebinOK  = root["digitizers"].toArray()[0].toObject()["channels"].toArray()[0]
                          .toObject()["bins"].toInt() == 321;
              splitOK  = root["split"].toObject()["cols"].toInt() == 2;
            }
          }
          if( !(jsonBack && txtGone && rebinOK && splitOK) ){
            printf("MIGRATION: json=%d bak=%d rebin=%d split=%d\n", jsonBack, txtGone, rebinOK, splitOK);
            nSaveErr++;
          }
          QFile::remove(txtP + ".bak");   // leave a clean slate for the teardown save
        }
      }

      feedStop = true;
      for( auto & t : feeds ) t.join();
      feeds.clear();
      if( ss2 ) ss2->stopTimer();
      printf("phase 2 done: fed=%lu\n", fedCount.load());
      phase.stop();
      chaos.stop();
      delete ss2; ss2 = nullptr;
      delete dummy; dummy = nullptr;
      delete w; w = nullptr;
      a.quit();
      return;
    }

    //--------------- phase 1 : the real MainWindow + real board
    SingleSpectra * ss = curSS(w);

    if( !acqStarted ){
      if( !ss ){                       // not yet built? kick OpenDigitizers (private slot)
        QMetaObject::invokeMethod( w, "OpenDigitizers", Qt::DirectConnection );
        ss = w->findChild<SingleSpectra *>();
      }
      if( !ss ){ printf("no SingleSpectra after OpenDigitizers\n"); phase.stop(); return; }

      QList<QPushButton *> bns = w->findChildren<QPushButton *>();
      QPushButton * bnStart = nullptr;
      for( QPushButton * b : bns ) if( b->text() == "Start ACQ" ) bnStart = b;
      if( !bnStart || !bnStart->isEnabled() ){
        printf("RESULT: board NOT connected (Start ACQ disabled)\n");
        phase.stop(); QTimer::singleShot(0, &a, [&a](){ a.exit(2); });
        return;
      }
      EnableFill(ss);
      QMetaObject::invokeMethod( w, "AutoRun", Qt::DirectConnection );
      acqStarted = true;
      printf("ACQ started, chaos for %d s on SingleSpectra %p\n", runSec, (void*)ss);
      return;
    }

    // progress report: totals must advance while the board streams
    if( ss ){
      static int d = 0, c = 0;
      unsigned long t = ss->GetTotal1D(d, c);
      if( t > 0 ) sawEvents = true;
      printf("p1 t=%3ds  total(d%d,c%d)=%lu  rebin=%d clear=%d combo=%d refresh=%d\n",
             elapsed, d, c, t, nRebin, nClear, nCombo, nRefresh);
      if( ++c >= 8 ) { c = 0; d++; }
    }

    if( elapsed >= runSec ){
      printf("phase 1 done: rebin=%d clear=%d combo=%d refresh=%d gate=%d gatePos=%d sawEvents=%d -> stop ACQ, start synthetic feed\n",
             nRebin, nClear, nCombo, nRefresh, nGate, nGateWithCounts, (int)sawEvents);
      QMetaObject::invokeMethod( w, "StopACQ", Qt::DirectConnection );

      dummy = new Digitizer2Gen();
      dummy->SetDummy(999);
      static Digitizer2Gen * darr[1];  // must outlive the ctor call: SingleSpectra keeps the pointer
      darr[0] = dummy;
      ss2 = new SingleSpectra(darr, 1, "/home/ryan/ExpData/solaris_test/testing");
      ss2->show();
      EnableFill(ss2);
      // mount a real 1D pane deterministically (channel combos have 65+ entries; the digitizer
      // combo few) so publish/gate paths run even when the random combo churn lands on 2D views
      for( QComboBox * cb : ss2->findChildren<QComboBox *>() ){
        if( cb->count() <= 64 ) continue;
        for( int i = 0; i < cb->count(); i++ ){          // pick ch-0, NOT index 1 (that is "Everything")
          const int dat = cb->itemData(i).toInt();
          if( dat == 0 ){ cb->setCurrentIndex( i );
            printf("mount: combo idx=%d data=%d\n", i, dat); break; }
        }
      }
      printf("mounted 1D widgets: %d\n", (int)ss2->findChildren<Histogram1D *>().size());
      ss2->startTimer();

      for( int ch = 0; ch < 16; ch++ ){
        feeds.emplace_back([ch](){
          unsigned long s = 0x9E3779B9u + 2654435761u * (unsigned)ch;
          auto xr = [&](){ s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; };
          while( !feedStop ){
            HitSummary h;
            h.energy       = (uint16_t)( xr() % 60000u );
            h.energy_short = h.energy;
            dummy->ringBuffer[ch].push(h);
            fedCount.fetch_add(1, std::memory_order_relaxed);
          }
        });
      }
      printf("phase 2: 16 feeders running on dummy board\n");
    }
  });
  phase.start();

  chaos.setInterval(80);
  QObject::connect( &chaos, &QTimer::timeout, [&](){
    if( !w ) return;
    SingleSpectra * ss = curSS(w);
    if( acqStarted && ss ) ChaosStep(w, ss);
  });
  chaos.start();

  int rc = a.exec();
  printf("RESULT: clean exit rc=%d sawEvents=%d gates=%d gatesWithCounts=%d keyErr=%d boundsErr=%d sumErr=%d saveErr=%d\n",
         rc, (int)sawEvents, nGate, nGateWithCounts, nKeyError, nBoundsError, nSumError, nSaveErr);
  return (rc == 0 && fedCount.load() > 0 && nGate > 0 && nGateWithCounts > 0 && nKeyError == 0 && nBoundsError == 0 && nSumError == 0 && nSaveErr == 0) ? 0 : 3;
}
