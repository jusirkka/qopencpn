#pragma once
#include <QXmlStreamReader>
#include <QHash>
#include "s52presentation.h"
#include "settings.h"

#define S52INSTR_LTYPE Private::LocationType
#define S52INSTR_STYPE Private::Instr_ValueType
#define S52HPGL_LTYPE Private::LocationType
#define S52HPGL_STYPE Private::HPGL_ValueType

#ifndef YY_TYPEDEF_YY_SCANNER_T
#define YY_TYPEDEF_YY_SCANNER_T
using yyscan_t = void *;
#endif

#define YYLLOC_DEFAULT(Current, Rhs, N) do if (N) {\
    (Current).pos = YYRHSLOC (Rhs, 1).pos;\
    (Current).prev_pos = YYRHSLOC (Rhs, 1).prev_pos;\
    } else {                                        \
    (Current).pos = YYRHSLOC (Rhs, 0).pos;\
    (Current).prev_pos = YYRHSLOC (Rhs, 0).prev_pos;\
    } while (0)

namespace Private {

// parser location and value types
struct LocationType {
  int prev_pos;
  int pos;
};

struct Instr_ValueType {
  QString v_string;
  char v_char;
  int v_int;
  float v_float;
};

struct HPGL_ValueType {
  char v_char;
  int v_int;
};

class Presentation: public QObject {

  Q_OBJECT

public:

  static Presentation* instance();

private:

  Presentation();

  void readObjectClasses();
  void readAttributes();
  void readChartSymbols();

  void readColorTables(QXmlStreamReader& reader);
  void readLookups(QXmlStreamReader& reader);
  void readLineStyles(QXmlStreamReader& reader);
  void readPatterns(QXmlStreamReader& reader);
  void readSymbols(QXmlStreamReader& reader);

  S52::ColorRef parseColorRef(QXmlStreamReader &reader);

  int parseInstruction(S52::Lookup* lup);

  quint32 m_nextSymbolIndex;

private slots:

  void setSimplifiedSymbols(bool);
  void setPlainBoundaries(bool);
  void setColorTable(Settings::ColorTable t);

public:

  void init();

  S52::Lookup::Type typeFilter(const S57::Object* obj) const;

  using IdentifierHash = QHash<QString, quint32>;
  using StringHash = QHash<quint32, QString>;

  IdentifierHash names;
  StringHash classDescriptions;

  using DescriptionMap = QMap<quint32, QString>;

  struct AttributeDescription {
    AttributeDescription(S57::Attribute::Type t, const QString& d)
      : type(t)
      , description(d) {}

    AttributeDescription() = default;

    S57::Attribute::Type type;
    QString description;
    DescriptionMap enumDescriptions;
  };
  using AttributeMap = QMap<quint32, AttributeDescription>;

  AttributeMap attributes;

  using ColorVector = QVector<QColor>;

  struct ColorTable {
    ColorTable(const QString& gfile)
      : graphicsFile(gfile) {}

    ColorTable() = default;

    QString graphicsFile;
    ColorVector colors;
  };

  QVector<ColorTable> colorTables;
  quint32 currentColorTable;


  using LookupVector = QVector<S52::Lookup*>;
  using LookupHash = QMultiHash<quint32, quint32>; // key: class code, value: vector index
  using LupIterator = QMultiHash<quint32, quint32>::iterator;
  using LookupTable = QMap<S52::Lookup::Type, LookupHash>;

  LookupVector lookups;
  LookupTable lookupTable;

  using LineStyleMap = QMap<quint32, S52::LineStyle>;
  LineStyleMap lineStyles;

  using PatternMap = QMap<quint32, S52::Pattern>;
  PatternMap patterns;

  using SymbolMap = QMap<quint32, S52::Symbol>;
  SymbolMap symbols;

  S52::Functions* functions;
  Settings* settings;
  bool simplifiedSymbols;
  bool plainBoundaries;
};

} // namespace Private


void s52hpgl_error(Private::LocationType*,
                   Private::Presentation*,
                   S52::LineStyle*,
                   yyscan_t, const char*);

void s52instr_error(Private::LocationType*,
                    Private::Presentation*,
                    S52::Lookup*,
                    yyscan_t, const char*);
