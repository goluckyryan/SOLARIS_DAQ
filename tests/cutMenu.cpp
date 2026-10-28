//^====================================================
//^ Regression: right-click -> Load Cut(s) -> Cancel must not crash.
//^ The old heap QMenu(WA_DeleteOnClose) died when exec() closed; the nested
//^ QFileDialog loop processed that deletion; the trailing selectedAction->text()
//^ dereference then read freed memory. Drive the exact path with synthetic keys:
//^ menu opens on a scripted right-press, 6x Down lands on "Load Cut(s)"
//^ (UnZoom, Log-Z, Clear hist., Toggle Stat., Rebin, Load), Return selects it,
//^ Escape cancels the dialog, then the function's tail runs the old crash site.
//^ Exit 0 = survived; any SIGSEGV/abort = the UAF is back.
//^====================================================
#include <QApplication>
#include <QTimer>
#include <QKeyEvent>
#include <cstdio>

#include "Histogram2D.h"

static void keyTo( QWidget * w, int key ){
  if( !w ) return;
  QKeyEvent k( QEvent::KeyPress, key, Qt::NoModifier );
  QApplication::sendEvent( w, &k );
  QKeyEvent kr( QEvent::KeyRelease, key, Qt::NoModifier );
  QApplication::sendEvent( w, &kr );
}

int main( int argc, char * argv[] ){
  QApplication a(argc, argv);

  Histogram2D h("t","Ch","E",64,0,64,200,0,5000);
  h.resize(800,600);
  h.show();

  // press at a point inside the plot; rightMouseClickMenu opens menu->exec (nested loop)
  QTimer::singleShot(100, [&](){
    // a committed cut MUST exist: the trailing Delete-detection `selectedAction->text()`
    // only runs when numCut > 0 -- that dereference of the freed action is the crash.
    h.BeginCreateCut();
    auto click = [&]( int x, int y, bool dbl ){
      QPoint p(x,y);
      QMouseEvent::Type t = dbl ? QEvent::MouseButtonDblClick : QEvent::MouseButtonPress;
      QMouseEvent ev( t, QPointF(p), h.mapToGlobal(p), h.mapToGlobal(p),
                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier );
      QApplication::sendEvent(&h, &ev);
    };
    click(300,200,false); click(400,200,false); click(350,300,true);
    printf("cuts before menu: %d\n", (int)h.GetCuts().size());

    QPoint p(400,300);
    QMouseEvent pr( QEvent::MouseButtonPress, QPointF(p), h.mapToGlobal(p), h.mapToGlobal(p),
                    Qt::RightButton, Qt::RightButton, Qt::NoModifier );
    QApplication::sendEvent(&h, &pr );   // blocks inside rightMouseClickMenu until menu+dialog finish
    printf("press handler returned\n");
  });

  // drive the menu from within its exec() loop
  QTimer::singleShot(400, [&](){
    QWidget * pop = QApplication::activePopupWidget();
    if( !pop ){ printf("NO POPUP\n"); a.exit(4); return; }
    for( int i = 0; i < 6; i++ ) keyTo( pop, Qt::Key_Down );   // first Down picks item 1; Load Cut(s) is item 6
    keyTo( pop, Qt::Key_Return );
  });

  // cancel whatever dialog the a8 branch opened
  QTimer::singleShot(800, [&](){
    QWidget * modal = QApplication::activeModalWidget();
    if( !modal ){ printf("NO MODAL DIALOG OPENED\n"); a.exit(5); return; }
    keyTo( modal, Qt::Key_Escape );
  });

  // we are back inside rightMouseClickMenu's tail -- let it return, then leave cleanly
  QTimer::singleShot(1400, [&](){ printf("SURVIVED cancel\n"); a.quit(); });
  QTimer::singleShot(6000, [&](){ printf("TIMEOUT/HANG\n"); a.exit(6); });

  int rc = a.exec();
  printf("exit rc=%d\n", rc);
  return rc;
}
