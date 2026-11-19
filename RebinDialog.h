#ifndef REBIN_DIALOG_H
#define REBIN_DIALOG_H

#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QStringList>
#include <array>
#include <vector>

/// One axis of the rebin dialog: the three labels shown, and the binning -- filled with the
/// current values before the dialog, replaced by what the user typed when it returns true.
struct RebinAxis {
  QString binLabel, minLabel, maxLabel;
  int     nBin;
  double  lo, hi;
};

/// The right-click "Rebin" dialog of Histogram1D (one axis) and Histogram2D (two axes). Shows the
/// current binning, checks what was typed -- numbers, at least one bin, max above min, every
/// problem on every axis reported at once -- and returns true on OK with the axes updated.
inline bool AskRebin(QWidget * parent, std::vector<RebinAxis> & axes){

  QDialog dialog(parent);
  dialog.setWindowTitle("Rebin histogram");
  QFormLayout layout(&dialog);

  QLabel * info = new QLabel("This will also clear histogram!!", &dialog);
  info->setStyleSheet("color:red;");
  layout.addRow(info);

  std::vector<std::array<QLineEdit *, 3>> edit(axes.size());
  for( size_t k = 0; k < axes.size(); k++ ){
    const QString label[3] = { axes[k].binLabel, axes[k].minLabel, axes[k].maxLabel };
    const QString value[3] = { QString::number(axes[k].nBin), QString::number(axes[k].lo), QString::number(axes[k].hi) };
    for( int i = 0; i < 3; i++ ){
      edit[k][i] = new QLineEdit(value[i], &dialog);
      layout.addRow(label[i] + " : ", edit[k][i]);
    }
  }

  QLabel * msg = new QLabel(&dialog);
  msg->setStyleSheet("color:red;");
  layout.addRow(msg);

  QDialogButtonBox buttonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, &dialog);
  layout.addRow(&buttonBox);

  std::vector<std::array<double, 3>> number(axes.size());

  QObject::connect(&buttonBox, &QDialogButtonBox::accepted, [&](){
    for( size_t k = 0; k < axes.size(); k++ ){
      const QString label[3] = { axes[k].binLabel, axes[k].minLabel, axes[k].maxLabel };
      for( int i = 0; i < 3; i++ ){
        bool ok = false;
        number[k][i] = edit[k][i]->text().toDouble(&ok);
        if( !ok ){ msg->setText(label[i] + " is invalid."); return; }
      }
    }
    QStringList err;
    for( size_t k = 0; k < axes.size(); k++ ){
      if( number[k][0] < 1 )            err << axes[k].binLabel + " must be at least 1";
      if( number[k][2] <= number[k][1] ) err << axes[k].maxLabel + " is not larger than " + axes[k].minLabel;
    }
    if( !err.isEmpty() ){ msg->setText(err.join("; ")); return; }
    dialog.accept();
  });
  QObject::connect(&buttonBox, &QDialogButtonBox::rejected, [&](){ dialog.reject(); });

  if( dialog.exec() != QDialog::Accepted ) return false;

  for( size_t k = 0; k < axes.size(); k++ ){
    axes[k].nBin = (int) number[k][0];
    axes[k].lo   = number[k][1];
    axes[k].hi   = number[k][2];
  }
  return true;
}

#endif
