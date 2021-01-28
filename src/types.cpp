#include "types.h"
#include <cmath>
#include <QRegularExpression>
#include "Geodesic.hpp"
#include <QDebug>

double Angle::degrees() const {
  return radians * DEGS_PER_RAD;
}

Angle Angle::fromRadians(double r) {
  return Angle(r);
}

Angle Angle::fromDegrees(double d) {
  return Angle(d * RADS_PER_DEG);
}

Angle Angle::ASin(double s) {
  return Angle(asin(s));
}

Angle Angle::ATan2(double y, double x) {
  return Angle(atan2(y, x));
}

Angle operator+ (const Angle& a, const Angle& b) {
  return Angle(a.radians + b.radians);
}

Angle operator- (const Angle& a, const Angle& b) {
  return Angle(a.radians - b.radians);
}

Angle operator- (const Angle& a) {
  return Angle(- a.radians);
}

Angle::Angle(double r): m_valid(true) {
  // normalize - valid implies normalized
  while (r > PI) {
    r -= 2 * PI;
  }
  while (r < -PI) {
    r += 2 * PI;
  }
  radians = r;
}

WGS84Point WGS84Point::fromLL(double lng, double lat) {
  return WGS84Point(lng, lat);
}

WGS84Point WGS84Point::fromLLRadians(double lng, double lat) {
  return WGS84Point(lng * DEGS_PER_RAD, lat * DEGS_PER_RAD);
}

WGS84Point WGS84Point::parseISO6709(const QString& loc) {
  // +27.5916+086.5640+8850CRSWGS_84/
  QRegularExpression coord("([+-]\\d+(?:\\.\\d+)?)(.*)");
  QVector<double> cs;
  QRegularExpressionMatch m;
  QString rest;
  for (m = coord.match(loc); m.hasMatch(); m = coord.match(m.captured(2))) {
      cs.append(m.captured(1).toDouble());
      rest = m.captured(2);
  }
  if (cs.length() < 2) {
      return WGS84Point();
  }

  double lat = cs[0];
  double lng = cs[1];

  QString datum("WGS_84");
  QRegularExpression datumRe("CRS([^/]+)(/.*)");
  m = datumRe.match(rest);
  if (m.hasMatch()) {
      datum = m.captured(1);
      rest = m.captured(2);
  }

  if (rest != "/" || datum != "WGS_84") {
      return WGS84Point();
  }

  return WGS84Point(lng, lat);
}

static double sexas(double r) {
    r = std::abs(r);
    return  60 * (r - int(r));
}


QString WGS84Point::print(Units units) const {
  if (!m_Valid) return "N/A";

  const QChar z('0');

  switch (units) {
  case Units::DegMinSec: {
    // 50°40'46“N 024°48'26“E
    const QString s("%1°%2‘%3“%4");
    QString r;
    r += s.arg(degreesLat(), 2, 10, z).arg(minutesLat(), 2, 10, z)
            .arg(secondsLat(), 2, 10, z).arg(northern() ? "N" : "S");
    r += " ";
    r += s.arg(degreesLng(), 3, 10, z).arg(minutesLng(), 2, 10, z)
         .arg(secondsLng(), 2, 10, z).arg(eastern() ? "E" : "W");

    return r;
  }
  case Units::DegMin: {
    // 50°40.7667'N 024°48.4333'E
    const QString s("%1°%2‘%3");
    QString r;
    r += s.arg(degreesLat(), 2, 10, z).arg(sexas(m_Latitude), 2, 'f', 4, z)
        .arg(northern() ? "N" : "S");
    r += " ";
    r += s.arg(degreesLng(), 3, 10, z).arg(sexas(m_Longitude), 2, 'f', 4, z)
        .arg(eastern() ? "E" : "W");

    return r;
  }
  case Units::Deg: {
    // 50.679445° 024.807222°
    const QString s("%1°");
    QString r;
    r += s.arg(m_Latitude, 2, 'f', 6, z);
    r += " ";
    r += s.arg(m_Longitude, 3, 'f', 6, z);

    return r;
  }
  default:
    return "N/A";
  }

}

QString WGS84Point::toISO6709() const {
  if (!m_Valid) return "N/A";
  // +27.5916+086.5640CRSWGS_84/
  QChar z('0');
  QString s("%1%2%3%4CRSWGS_84/"); // sign lat, abs lat, sign lng, abs lng
  return s.arg(m_Latitude < 0 ? '-' : '+').arg(std::abs(m_Latitude), 0, 'f', 10, z)
      .arg(m_Longitude < 0 ? '-' : '+').arg(std::abs(m_Longitude), 0, 'f', 10, z);
}

double WGS84Point::radiansLng() const {
  return RADS_PER_DEG * m_Longitude;
}

double WGS84Point::radiansLat() const {
  return RADS_PER_DEG * m_Latitude;
}


ushort WGS84Point::degreesLat() const {
  return std::abs(m_Latitude);
}

ushort WGS84Point::degreesLng() const {
  return std::abs(m_Longitude);
}

ushort WGS84Point::minutesLat() const {
  return sexas(m_Latitude);
}

ushort WGS84Point::minutesLng() const {
  return sexas(m_Longitude);
}

ushort WGS84Point::secondsLat() const {
  return sexas(sexas(m_Latitude));
}

ushort WGS84Point::secondsLng() const {
  return sexas(sexas(m_Longitude));
}

double WGS84Point::normalizeAngle(double a) {
  while (a > 180) {
    a -= 360;
  }
  while (a < -180) {
    a += 360;
  }
  return a;
}

WGS84Point::WGS84Point(double lng, double lat): m_Valid(true) {
  // normalize angles here - valid implies normalized
  lng = normalizeAngle(lng);
  lat = normalizeAngle(lat);
  if (lat > 90) {
    lat = 90 - lat;
  } else if (lat < - 90) {
    lat = - 90 - lat;
  }
  m_Longitude = lng;
  m_Latitude = lat;
}

double WGS84Point::lng(const WGS84Point& ref) const {
  if (m_Longitude < ref.lng()) {
    return m_Longitude + 360.;
  }
  return m_Longitude;
}



bool operator!= (const WGS84Point& a, const WGS84Point& b) {
  if (a.m_Valid != b.m_Valid) return true;
  if (!a.m_Valid) return false; // both invalid

  const WGS84Point d(a.m_Longitude - b.m_Longitude, a.m_Latitude - b.m_Latitude);

  if (d.m_Longitude > .1 || d.m_Latitude > .1) return true;

  const double c = cos(a.radiansLat());
  const double dlat2 = d.radiansLat() * d.radiansLat();
  const double dlng2 = d.radiansLng() * d.radiansLng();

  // within 20 meters?
  return sqrt(dlat2 + c * c * dlng2) * WGS84Point::semimajor_axis > WGS84Point::equality_radius;
}

bool operator== (const WGS84Point& a, const WGS84Point& b) {
  return !(a != b);
}

WGS84Bearing WGS84Bearing::fromMeters(double m, const Angle& a) {
  return WGS84Bearing(m, a);
}

WGS84Bearing WGS84Bearing::fromNM(double nm, const Angle& a) {
  return WGS84Bearing(nm * 1852., a);
}

WGS84Bearing::WGS84Bearing(double m, const Angle& a)
  : m_meters(m)
  , m_radians(a.radians)
  , m_valid(a.valid() && m > 0.)
{}


WGS84Bearing operator- (const WGS84Bearing& b) {
  return WGS84Bearing(b.meters(), Angle::fromRadians(b.radians() + M_PI));
}

WGS84Point operator- (const WGS84Point& a, const WGS84Bearing& b) {
  return a + (-b);
}

WGS84Point operator+ (const WGS84Point& a, const WGS84Bearing& b) {
  double lng2;
  double lat2;
  GeographicLib::Geodesic::WGS84().Direct(a.lat(), a.lng(), b.degrees(), b.meters(),
                                          lat2, lng2);
  return WGS84Point::fromLL(lng2, lat2);
}

WGS84Bearing operator- (const WGS84Point& p2, const WGS84Point& p1) {
  double s12;
  double azi1;
  double azi2;
  GeographicLib::Geodesic::WGS84().Inverse(p1.lat(), p1.lng(), p2.lat(), p2.lng(),
                                           s12, azi1, azi2);
  return WGS84Bearing::fromMeters(s12, Angle::fromDegrees(azi1));
}

WGS84Bearing operator* (double s, const WGS84Bearing& b) {
  if (s < 0){
    return WGS84Bearing::fromMeters( - s * b.meters(), Angle::fromRadians(b.radians() + M_PI));
  }
  return WGS84Bearing::fromMeters(s * b.meters(), Angle::fromRadians(b.radians()));
}

