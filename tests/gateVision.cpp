//^====================================================
//^ Gate vision check: builds a Histogram1D offscreen, feeds it a fake spectrum, adds two
//^ gates and grabs the widget to PNG + pixel probes. Proves where the band, the stacked
//^ labels and their colors actually land.
//^   run: QT_QPA_PLATFORM=offscreen ./tests/gateVision
//^   image: /tmp/opencode/gate.png
//^====================================================
#include <QApplication>
#include <QImage>
#include <QPixmap>
#include <cstdio>

#include "Histogram1D.h"

static void bbox( const QImage & img, const char * tag,
                  bool (*hit)(int r,int g,int b) ){
  int x0=1<<30,x1=-1,y0=1<<30,y1=-1,n=0;
  for( int y = 0; y < img.height(); y++ )
    for( int x = 0; x < img.width(); x++ ){
      const QRgb p = img.pixel(x,y);
      if( hit(qRed(p),qGreen(p),qBlue(p)) ){ n++;
        x0=qMin(x0,x); x1=qMax(x1,x); y0=qMin(y0,y); y1=qMax(y1,y); }
    }
  if( !n ) printf("%-14s NOT FOUND\n", tag);
  else printf("%-14s n=%d  x[%d..%d] y[%d..%d]\n", tag, n, x0, x1, y0, y1);
}

static bool redText(int r,int g,int b)    { return r>=115&&r<=165 && g<70 && b<70; }          // (200,0,0)/1.4
static bool magentaText(int r,int g,int b){ return r>=95&&r<=140 && b>=95&&b<=140 && qAbs(r-b)<30 && g<70; } // (170,0,170)/1.4
static bool redBand(int r,int g,int b)    { return r>235 && r-g>18 && qAbs(g-b)<4 && g>200 && g<240; }       // (248,225,225)

int main( int argc, char * argv[] ){
  qInstallMessageHandler( [](QtMsgType, const QMessageLogContext &, const QString &){} );
  QApplication a(argc, argv);

  Histogram1D h("T","E",512,0,5000);
  h.resize(700,450);
  h.show();

  static uint32_t y[512];
  for( int i = 0; i < 512; i++ ) y[i] = 100 + (i % 50);
  h.SetBinContents(y, 512, 60000, 10, 10, 0);
  h.UpdatePlot();       // lays out the axis rect, so pixelToCoord in AddGate is meaningful

  const int k0 = h.AddGate(QPoint(350, 225));      // red, centered on pixel 350
  h.SetGateRate(k0, true, 412.3);
  const int k1 = h.AddGate(QPoint(600, 225));      // magenta, near the right edge
  h.SetGateRate(k1, true, 17.5);
  h.replot();

  const QPixmap pm = h.grab();
  pm.save("/tmp/opencode/gate.png");
  const QImage img = pm.toImage();
  const QRect ar = h.axisRect()->rect();
  printf("image %dx%d   axisRect y[%d..%d]   gate0 center px=%d  gate1 center px=%d\n",
         img.width(), img.height(), ar.top(), ar.bottom(),
         (int)h.xAxis->coordToPixel(0.5*(h.GateLow(k0)+h.GateHigh(k0))),
         (int)h.xAxis->coordToPixel(0.5*(h.GateLow(k1)+h.GateHigh(k1))) );
  printf("expected label baselines: k0 at y=%d (10%% up), k1 at y=%d (15%% up)\n",
         ar.bottom() - (int)(0.10*ar.height()), ar.bottom() - (int)(0.15*ar.height()) );

  bbox(img, "red label",    redText);
  bbox(img, "magenta label",magentaText);
  bbox(img, "red band tint",redBand);
  return 0;
}
