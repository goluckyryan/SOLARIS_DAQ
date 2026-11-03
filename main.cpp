#include "mainwindow.h"

#include <QApplication>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QFile>
#include <QFileInfo>

int main(int argc, char *argv[]){

    printf("######### Starting SOLARIS DAQ....\n");

    QApplication a(argc, argv);

    bool isLock = false;
    int pid = 0;

    QFile lockFile(DAQLockFile);
    if( lockFile.open(QIODevice::Text | QIODevice::ReadOnly) ){
        QTextStream in(&lockFile);
        QString line = in.readLine();
        isLock = line.toInt();
        lockFile.close();
    }
    QFile pidFile(PIDFile);
    if( pidFile.open(QIODevice::Text | QIODevice::ReadOnly)){
        QTextStream in(&pidFile);
        QString line = in.readLine();
        pid = line.toInt();
        pidFile.close();
    }

    if( isLock ) {

        qDebug() << "The DAQ program is already opened. PID is " + QString::number(pid) + ", and delete the " + DAQLockFile ;

        QMessageBox msgBox;
        msgBox.setWindowTitle("Oopss....");
        msgBox.setText("The DAQ program is already opened, or crashed perviously. \nPID is " + QString::number(pid) + "\n If it is still running, kill it and delete the " + DAQLockFile + "\n or click the \"Kill\" button (kills only if that PID is really a DAQ)");
        msgBox.setIcon(QMessageBox::Information);
    
        QPushButton * kill = msgBox.addButton("Kill and Open New", QMessageBox::AcceptRole);

        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.setDefaultButton(QMessageBox::Ok);

        msgBox.exec();

        if(msgBox.clickedButton() == kill){
            remove(DAQLockFile);
            /// Audit #17: the kernel may have recycled this PID since the crash -- killing it
            /// blindly could SIGKILL someone's editor. /proc/<pid>/exe resolves only for
            /// processes we may inspect, so require it to be this same binary. pid == 0
            /// (missing/unreadable pid file) is refused too: `kill -9 0` would signal the
            /// caller's whole process group -- this terminal session included.
            QString exe = pid > 0 ? QFile::symLinkTarget("/proc/" + QString::number(pid) + "/exe") : QString();
            if( !exe.isEmpty() && QFileInfo(exe).canonicalFilePath() == QFileInfo(QCoreApplication::applicationFilePath()).canonicalFilePath() ){
                QProcess::execute("kill", QStringList() << "-9" << QString::number(pid));
            }else{
                printf("######### PID %d is not a running SOLARIS_DAQ (recycled or gone). Lock removed, NOT killing.\n", pid);
            }
        }else{
            return 0;
        }
    }

    if( !lockFile.open(QIODevice::Text | QIODevice::WriteOnly) )
      printf("######### could not open lock file %s -- stale-instance guard disabled\n",
             qPrintable(lockFile.fileName()));
    lockFile.write( "1" );
    lockFile.close();

    if( !pidFile.open(QIODevice::Text | QIODevice::WriteOnly) )
      printf("######### could not open pid file %s -- stale-instance guard disabled\n",
             qPrintable(pidFile.fileName()));
    pidFile.write(  QString::number(QCoreApplication::applicationPid() ).toStdString().c_str() );
    pidFile.close();

    printf("######### Open Main Window...\n");
    MainWindow w;
    w.show();
    return a.exec();

}
