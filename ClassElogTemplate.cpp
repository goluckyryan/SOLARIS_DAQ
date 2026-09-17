#include "ClassElogTemplate.h"

#include <QFile>
#include <QTextStream>

/// a token is <Name>, where Name starts with a letter and holds no space, '/' or '='.
/// that way </b>, <br /> and <font style="..."> are not tokens and pass through untouched.
static const int MaxTokenLength = 64;

static bool IsTokenChar(QChar c){
  return c.isLetterOrNumber() || c == '_' || c == ':' || c == '.' || c == '-';
}

/// a bare HTML tag, <b> or <hr>, also fits the token shape. Those must stay markup,
/// otherwise every <b>bold</b> in a template would be reported as an unknown variable.
/// A tag with attributes, <font style="...">, already fails the token shape.
static bool IsHtmlTag(const QString & name){
  static const QStringList tag = {
    "a", "b", "big", "blockquote", "br", "center", "code", "div", "em", "font", "hr",
    "h1", "h2", "h3", "h4", "h5", "h6", "i", "img", "li", "ol", "p", "pre", "s",
    "small", "span", "strong", "sub", "sup", "table", "tbody", "td", "th", "thead",
    "tr", "tt", "u", "ul"
  };
  return tag.contains(name.toLower());
}

/// read the token starting at index start ('<'). returns its name, or a null QString
/// when there is no token here. end is set to the index just after the '>'.
static QString ReadToken(const QString & line, int start, int & end){
  if( start + 1 >= line.length() ) return QString();
  if( !line.at(start + 1).isLetter() ) return QString();

  for( int i = start + 1; i < line.length() && i - start <= MaxTokenLength ; i++ ){
    const QChar c = line.at(i);
    if( c == '>' ){
      const QString name = line.mid(start + 1, i - start - 1);
      if( IsHtmlTag(name) ) return QString();
      end = i + 1;
      return name;
    }
    if( !IsTokenChar(c) ) return QString();
  }
  return QString();
}

/// split "Bd12" into the prefix "Bd" and the number 12. index is -1 when no number is given.
static bool SplitIndexed(const QString & text, const QString & prefix, int & index){
  if( !text.startsWith(prefix, Qt::CaseInsensitive) ) return false;
  const QString num = text.mid(prefix.length());
  if( num.isEmpty() ){
    index = -1;
    return true;
  }
  bool ok = false;
  index = num.toInt(&ok);
  return ok;
}

//^#===================================================== ElogTemplate

ElogTemplate::ElogTemplate(){
  ClearVars();
}

QString ElogTemplate::Escape(const QString & text){
  return text.toHtmlEscaped();
}

void ElogTemplate::ClearVars(){
  varName.clear();
  varValue.clear();
}

void ElogTemplate::SetVar(const QString & name, const QString & value){
  const int index = varName.indexOf(name);
  if( index >= 0 ){
    varValue[index] = value;
  }else{
    varName  << name;
    varValue << value;
  }
}

void ElogTemplate::SetVar(const QString & name, int value){
  SetVar(name, QString::number(value));
}

bool ElogTemplate::Has(ElogSection sec) const {
  return Get(sec).exist;
}

//^===================================================== parsing

bool ElogTemplate::Load(const QString & filePath){

  this->filePath = filePath;
  errorMsg = "";
  startRun = Section();
  stopRun  = Section();

  QFile file(filePath);
  if( !file.open(QIODevice::Text | QIODevice::ReadOnly) ){
    errorMsg = "cannot open " + filePath;
    return false;
  }

  Section * current = nullptr;

  QTextStream in(&file);
  while( !in.atEnd() ){

    QString line = in.readLine();
    /// a line editor may leave a CR behind, it would end up in the middle of the HTML
    while( line.endsWith('\r') ) line.chop(1);

    const QString trimmed = line.trimmed();

    //-------- section marker
    if( trimmed.startsWith("#===") ){
      const QString name = trimmed.mid(4).trimmed();
      if( name.compare("start Run", Qt::CaseInsensitive) == 0 ){
        current = &startRun;
        current->exist = true;
      }else if( name.compare("stop Run", Qt::CaseInsensitive) == 0 ){
        current = &stopRun;
        current->exist = true;
      }else{
        current = nullptr; /// an unknown section is skipped, not appended to the previous one
        errorMsg = "unknown section \"" + name + "\"";
      }
      continue;
    }

    if( current == nullptr ) continue; /// header comments, before any section

    //-------- directives and comments
    if( trimmed.startsWith("#") ){
      if( trimmed.startsWith("#Subject:", Qt::CaseInsensitive) ){
        current->subject = trimmed.mid(9).trimmed();
      }else if( trimmed.startsWith("#Category:", Qt::CaseInsensitive) ){
        current->category = trimmed.mid(10).trimmed();
      }
      continue; /// every other # line is a comment
    }

    //-------- body. "\#" escapes a leading # so a line can start with one
    if( line.startsWith("\\#") ) line = line.mid(1);

    current->body << line;
  }

  file.close();

  /// trailing empty lines would show up as empty HTML lines at the end of the entry
  for( Section * sec : {&startRun, &stopRun} ){
    while( !sec->body.isEmpty() && sec->body.last().trimmed().isEmpty() ) sec->body.removeLast();
  }

  if( !startRun.exist && !stopRun.exist ){
    errorMsg = filePath + " holds no \"#=== start Run\" or \"#=== stop Run\" section";
    return false;
  }

  return true;
}

//^===================================================== rendering

void ElogTemplate::ScanLoop(const QString & line, bool & loopBd, bool & loopCh, int & fixedBd){

  loopBd  = false;
  loopCh  = false;
  fixedBd = 0;

  for( int i = 0; i < line.length(); i++ ){

    if( line.at(i) != '<' ) continue;

    int end = 0;
    const QString name = ReadToken(line, i, end);
    if( name.isNull() ) continue;
    i = end - 1;

    /// the bare loop indexes
    if( name.compare("Bd", Qt::CaseInsensitive) == 0 ){ loopBd = true; continue; }
    if( name.compare("Ch", Qt::CaseInsensitive) == 0 ){ loopCh = true; loopBd = true; continue; }

    const QStringList part = name.split(':');
    if( part.size() < 2 ) continue;

    int bdIndex = 0;
    if( !SplitIndexed(part.at(0), "Bd", bdIndex) ) continue;

    if( bdIndex < 0 ) loopBd = true;

    /// <Bd:Ch:Prop> or <Bd:Ch7:Prop>
    if( part.size() >= 3 ){
      int chIndex = 0;
      if( SplitIndexed(part.at(1), "Ch", chIndex) && chIndex < 0 ){
        loopCh = true;
        if( bdIndex >= 0 ) fixedBd = bdIndex;
      }
    }
  }
}

QString ElogTemplate::Resolve(const QString & name, int bd, int ch, bool & ok) const {

  ok = true;

  //-------- the loop indexes themselves
  if( name.compare("Bd", Qt::CaseInsensitive) == 0 && bd >= 0 ) return QString::number(bd);
  if( name.compare("Ch", Qt::CaseInsensitive) == 0 && ch >= 0 ) return QString::number(ch);

  const QStringList part = name.split(':');

  //-------- <Bd:Prop> / <Bd:Ch:Prop>
  int bdIndex = 0;
  if( part.size() >= 2 && SplitIndexed(part.at(0), "Bd", bdIndex) ){

    if( bdIndex < 0 ) bdIndex = bd;
    if( bdIndex < 0 ){ ok = false; return QString(); } /// <Bd:...> outside a board loop

    int chIndex = 0;
    if( part.size() >= 3 && SplitIndexed(part.at(1), "Ch", chIndex) ){

      if( chIndex < 0 ) chIndex = ch;
      if( chIndex < 0 || !chVar ){ ok = false; return QString(); }

      /// the property can hold a ':' itself, keep everything after "Bd:Ch:"
      const QString prop = part.mid(2).join(':');
      return chVar(bdIndex, chIndex, prop, ok);
    }

    if( !boardVar ){ ok = false; return QString(); }
    return boardVar(bdIndex, part.mid(1).join(':'), ok);
  }

  //-------- a plain variable
  for( int i = 0; i < varName.size(); i++ ){
    if( varName.at(i).compare(name, Qt::CaseInsensitive) == 0 ) return varValue.at(i);
  }

  ok = false;
  return QString();
}

QString ElogTemplate::ExpandLine(const QString & line, int bd, int ch, QStringList * unresolved) const {

  QString out;

  for( int i = 0; i < line.length(); i++ ){

    const QChar c = line.at(i);
    if( c != '<' ){
      out += c;
      continue;
    }

    int end = 0;
    const QString name = ReadToken(line, i, end);
    if( name.isNull() ){ /// not a token, e.g. <br />, <b>, <font ...>
      out += c;
      continue;
    }

    bool ok = false;
    const QString value = Resolve(name, bd, ch, ok);

    if( ok ){
      out += Escape(value);
    }else{
      out += line.mid(i, end - i); /// keep the token, so a typo is visible in the entry
      if( unresolved && !unresolved->contains(name) ) *unresolved << name;
    }

    i = end - 1;
  }

  return out;
}

QString ElogTemplate::Render(ElogSection sec, QStringList * unresolved) const {

  const Section & section = Get(sec);
  if( !section.exist ) return QString();

  const int nBd = nBoard ? nBoard() : 0;

  QStringList out;

  for( const QString & line : section.body ){

    bool loopBd = false, loopCh = false;
    int fixedBd = 0;
    ScanLoop(line, loopBd, loopCh, fixedBd);

    if( !loopBd && !loopCh ){
      out << ExpandLine(line, -1, -1, unresolved);
      continue;
    }

    if( loopCh && !loopBd ){ /// <Bd3:Ch:Prop>, only the channels of one board
      if( fixedBd >= nBd || (boardValid && !boardValid(fixedBd)) ) continue;
      const int nCh = nChannel ? nChannel(fixedBd) : 0;
      for( int ch = 0; ch < nCh; ch++ ) out << ExpandLine(line, fixedBd, ch, unresolved);
      continue;
    }

    for( int bd = 0; bd < nBd; bd++ ){
      if( boardValid && !boardValid(bd) ) continue;
      if( !loopCh ){
        out << ExpandLine(line, bd, -1, unresolved);
      }else{
        const int nCh = nChannel ? nChannel(bd) : 0;
        for( int ch = 0; ch < nCh; ch++ ) out << ExpandLine(line, bd, ch, unresolved);
      }
    }
  }

  return out.join("<br />\n");
}

QString ElogTemplate::Subject(ElogSection sec, QStringList * unresolved) const {
  const QString & text = Get(sec).subject;
  if( text.isEmpty() ) return QString();
  return ExpandLine(text, -1, -1, unresolved);
}

QString ElogTemplate::Category(ElogSection sec, QStringList * unresolved) const {
  const QString & text = Get(sec).category;
  if( text.isEmpty() ) return QString();
  return ExpandLine(text, -1, -1, unresolved);
}

//^===================================================== the shipped default

QString ElogTemplate::DefaultTemplate(){
  return
R"(#============================================================================
# SOLARIS DAQ elog template
#
# "#=== start Run" and "#=== stop Run" open the two entries. Every other line
# starting with # is a comment, use \# for a line that really starts with a #.
#
# <Name> is replaced by a variable. Anything else is passed through, so HTML
# like <br />, <b>bold</b> or <font style="color : red;">red</font> still works.
# An unknown <Token> is left in the entry and reported in the DAQ log panel.
#
# A line holding <Bd:Something>    is repeated once per digitizer.
# A line holding <Bd:Ch:Something> is repeated once per digitizer and channel.
# <Bd3:Something> / <Bd3:Ch7:Something> pick one board / channel and do not repeat.
# Inside a repeated line, <Bd> is the board index and <Ch> the channel index.
#
# Run variables
#   <ExpName> <ElogName> <RunID> <RunIDStr> <StartTime> <StopTime> <Duration>
#   <StartComment> <StopComment> <RunComment> <DataFormat> <AutoRun>
#   <FilePath> <NumberOfFile> <TotalFileSize> <TotalFileSizeMB> <TotalFileSizeByte>
#   <NumberOfBoard> <Now> <Host>
#
# Board variables
#   <Bd:SN> <Bd:Model> <Bd:FPGAType> <Bd:FPGAVer> <Bd:NChannel>
#   <Bd:FileSize> <Bd:FileSizeMB> <Bd:NumberOfFile> <Bd:FileName>
#   plus any board parameter, e.g. <Bd:TestPulsePeriod>
#
# Channel variables
#   <Bd:Ch:TrigRate> <Bd:Ch:AcceptRate> <Bd:Ch:SavedCount> <Bd:Ch:Realtime>
#   plus any channel parameter, e.g. <Bd:Ch:TriggerThreshold> <Bd:Ch:ChRecordLengthT>
#
# The parameter names are the CAEN names, as saved in the *XSetting_*.dat file.
#============================================================================

#=== start Run
#Subject: Run-<RunIDStr>
#Category: Run
=============== Run-<RunIDStr>
<StartTime>
comment : <StartComment>
----------------------------------------------

#=== stop Run
<StopTime>
FileSize (<Bd:SN>): <Bd:FileSizeMB> MB
comment : <StopComment>
======================
)";
}
