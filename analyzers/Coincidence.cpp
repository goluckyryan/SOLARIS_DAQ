#include "../Analysis.h"

/// Coincidence between two channels chosen at runtime.
///
/// Detector-agnostic on purpose: because the two channels are picked from the GUI rather than
/// compiled in, this works on any setup and is a reasonable first thing to look at on real beam.
///
/// The multiplicity and tB-tA plots cost almost nothing and are what actually tell you whether the
/// event builder's time window is set sensibly: dt should sit inside the window, and multiplicity
/// should broaden as the window is opened up.

class Coincidence : public Analysis {

  //^---- set by the host through ChannelParam; only touched on the analysis thread
  int digiA = 0, chA = 0;
  int digiB = 0, chB = 1;

  int hEE    = -1;
  int hTDiff = -1;
  int hMult  = -1;

public:

  void Declare(HistRegistry & reg, const AnalysisContext & ctx) override {

    reg.ChannelParam("Channel A", &digiA, &chA);
    reg.ChannelParam("Channel B", &digiB, &chB);

    hEE = reg.Hist2D("E(A) vs E(B)", "E(A)", "E(B)",
                     256, 0, 30000, 256, 0, 30000, 0, 0);

    /// Span the coincidence window both ways, so a window that is too wide or too narrow shows up
    /// as a peak that is lost in a flat background or clipped at the edges.
    const double w = (double)(ctx.timeWindow > 0 ? ctx.timeWindow : 100);
    hTDiff = reg.Hist1D("tB - tA", "ns", 200, -w, w, 0, 1);

    hMult = reg.Hist1D("Multiplicity", "hits / event", 65, 0, 65, 1, 0);
  }

  void ProcessEvent(const BuiltHit * hits, int n, HistRegistry & reg) override {

    reg.Fill1(hMult, n);

    const BuiltHit * a = nullptr;
    const BuiltHit * b = nullptr;

    for( int i = 0; i < n; i++ ){
      if( hits[i].digi == digiA && hits[i].channel == chA ) a = &hits[i];
      if( hits[i].digi == digiB && hits[i].channel == chB ) b = &hits[i];
    }

    if( a == nullptr || b == nullptr ) return;

    reg.Fill2(hEE, a->energy, b->energy);
    /// Signed difference, so cast out of uint64 before subtracting.
    reg.Fill1(hTDiff, (double)b->timestamp - (double)a->timestamp);
  }
};

ANALYSIS_ENTRY(Coincidence, "Coincidence");
