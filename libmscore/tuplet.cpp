//=============================================================================
//  MuseScore
//  Music Composition & Notation
//
//  Copyright (C) 2002-2011 Werner Schweer
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License version 2
//  as published by the Free Software Foundation and appearing in
//  the file LICENCE.GPL
//=============================================================================

#include "tuplet.h"
#include "score.h"
#include "chord.h"
#include "note.h"
#include "xml.h"
#include "staff.h"
#include "style.h"
#include "text.h"
#include "element.h"
#include "undo.h"
#include "stem.h"

namespace Ms {

//---------------------------------------------------------
//   Tuplet
//---------------------------------------------------------

Tuplet::Tuplet(Score* s)
  : DurationElement(s)
      {
      setFlags(ELEMENT_MOVABLE | ELEMENT_SELECTABLE);
      _numberType   = Tuplet::SHOW_NUMBER;
      _bracketType  = Tuplet::AUTO_BRACKET;
      _number       = 0;
      _hasBracket   = false;
      _isUp         = true;
      _direction    = MScore::AUTO;
      }

Tuplet::Tuplet(const Tuplet& t)
   : DurationElement(t)
      {
      _tick         = t._tick;
      _numberType   = t._numberType;
      _bracketType  = t._bracketType;
      _hasBracket   = t._hasBracket;
      _ratio        = t._ratio;
      _baseLen      = t._baseLen;

      _direction    = t._direction;
      _isUp         = t._isUp;

      p1            = t.p1;
      p2            = t.p2;
      _p1           = t._p1;
      _p2           = t._p2;

      if (t._number)
            _number = new Text(*t._number);
      else
            _number = 0;
      }

//---------------------------------------------------------
//   ~Tuplet
//---------------------------------------------------------

Tuplet::~Tuplet()
      {
      //
      // delete all references
      //
      foreach(DurationElement* e, _elements)
            e->setTuplet(0);
      delete _number;
      }

//---------------------------------------------------------
//   setSelected
//---------------------------------------------------------

void Tuplet::setSelected(bool f)
      {
      Element::setSelected(f);
      if (_number)
            _number->setSelected(f);
      }

//---------------------------------------------------------
//   setVisible
//---------------------------------------------------------

void Tuplet::setVisible(bool f)
      {
      Element::setVisible(f);
      if (_number)
            _number->setVisible(f);
      }

//---------------------------------------------------------
//   layout
//---------------------------------------------------------

void Tuplet::layout()
      {
      if (_elements.empty()) {
            qDebug("Tuplet::layout(): tuplet is empty");
            return;
            }
      // is in a TAB without stems, skip any format: tuplets are not shown
      if (staff() && staff()->isTabStaff() && staff()->staffType()->slashStyle())
            return;

      qreal _spatium = spatium();
      if (_numberType != NO_TEXT) {
            if (_number == 0) {
                  _number = new Text(score());
                  _number->setTextStyleType(TEXT_STYLE_TUPLET);
                  _number->setParent(this);
                  _number->setVisible(visible());
                  }
            if (_numberType == SHOW_NUMBER)
                  _number->setText(QString("%1").arg(_ratio.numerator()));
            else
                  _number->setText(QString("%1:%2").arg(_ratio.numerator()).arg(_ratio.denominator()));
            }
      else {
            if (_number) {
                  if (_number->selected())
                        score()->deselect(_number);
                  delete _number;
                  _number = 0;
                  }
            }
      //
      // find out main direction
      //
      if (_direction == MScore::AUTO) {
            int up = 1;
            foreach(const DurationElement* e, _elements) {
                  if (e->type() == CHORD) {
                        const Chord* c = static_cast<const Chord*>(e);
                        if (c->stemDirection() != MScore::AUTO)
                              up += c->stemDirection() == MScore::UP ? 1000 : -1000;
                        else
                              up += c->up() ? 1 : -1;
                        }
                  else if (e->type() == TUPLET) {
                        // TODO
                        }
                  }
            _isUp = up > 0;
            }
      else
            _isUp = _direction == MScore::UP;

      //
      // set all elements to main direction
      //
      bool tupletContainsRest = false;
      foreach(DurationElement* e, _elements) {
            if (e->type() == REST) {
                  tupletContainsRest = true;
                  break;
                  }
            }

      const DurationElement* cr1 = _elements.front();
      while (cr1->type() == TUPLET) {
            const Tuplet* t = static_cast<const Tuplet*>(cr1);
            if (t->elements().empty())
                  break;
            cr1 = t->elements().front();
            }
      const DurationElement* cr2 = _elements.back();
      while (cr2->type() == TUPLET) {
            const Tuplet* t = static_cast<const Tuplet*>(cr2);
            if (t->elements().empty())
                  break;
            cr2 = t->elements().back();
            }

      //
      //   shall we draw a bracket?
      //
      if (_bracketType == AUTO_BRACKET) {
            _hasBracket = tupletContainsRest;
            if (!_hasBracket) {
                  foreach(DurationElement* e, _elements) {
                        if (e->type() == TUPLET) {
                              _hasBracket = true;
                              break;
                              }
                        else if (e->isChordRest()) {
                              ChordRest* cr = static_cast<ChordRest*>(e);
                              //
                              // maybe we should check for more than one beam
                              //
                              if (cr->beam() == 0) {
                                    _hasBracket = true;
                                    break;
                                    }
                              }
                        }
                  }
            }
      else
            _hasBracket = _bracketType != SHOW_NO_BRACKET;


      //
      //    calculate bracket start and end point p1 p2
      //
      qreal headDistance = _spatium * .75;
      if (_isUp) {
            p1       = cr1->abbox().topLeft();
            p1.ry() -= headDistance;
            p2       = cr2->abbox().topRight();
            p2.ry() -= headDistance;

            if (cr1->type() == CHORD) {
                  const Chord* chord1 = static_cast<const Chord*>(cr1);
                  Stem* stem = chord1->stem();

                  if (stem && chord1->up()) {
                        p1.setY(stem->abbox().y());
                        if (chord1->beam())
                              p1.setX(stem->abbox().x());
                        }
                  else if ((cr2->type() == CHORD) && stem && !chord1->up()) {
                        const Chord* chord2 = static_cast<const Chord*>(cr2);
                        Stem* stem2 = chord2->stem();
                        if (stem2) {
                              int l1 = chord1->upNote()->line();
                              int l2 = chord2->upNote()->line();
                              p1.ry() = stem2->abbox().top() + _spatium * .5 * (l1 - l2);
                              }
                        }
                  }

            if (cr2->type() == CHORD) {
                  const Chord* chord2 = static_cast<const Chord*>(cr2);
                  Stem* stem = chord2->stem();
                  if (stem && chord2->up()) {
                        p2.setY(stem->abbox().top());
                        if (chord2->beam())
                              p2.setX(stem->abbox().x());
                        }
                  else if ((cr1->type() == CHORD) && stem && !chord2->up()) {
                        const Chord* chord1 = static_cast<const Chord*>(cr1);
                        int l1 = chord1->upNote()->line();
                        int l2 = chord2->upNote()->line();
                        p2.ry() = p1.ry() + _spatium * .5 * (l2 - l1);
                        }
                  }
            //
            // special case: one of the bracket endpoints is
            // a rest
            //
            if (cr1->type() != CHORD && cr2->type() == CHORD) {
                  if (p2.y() < p1.y())
                        p1.setY(p2.y());
                  else
                        p2.setY(p1.y());
                  }
            else if (cr1->type() == CHORD && cr2->type() != CHORD) {
                  if (p1.y() < p2.y())
                        p2.setY(p1.y());
                  else
                        p1.setY(p2.y());
                  }

            // check for collisions

            int n = _elements.size();
            if (n >= 3) {
                  qreal d = (p2.y() - p1.y())/(p2.x() - p1.x());
                  for (int i = 1; i < (n-1); ++i) {
                        Element* e = _elements[i];
                        if (e->type() == CHORD) {
                              const Chord* chord = static_cast<const Chord*>(e);
                              const Stem* stem = chord->stem();
                              if (stem) {
                                    QRectF r(chord->up() ? stem->abbox() : chord->abbox());
                                    qreal y3 = r.top();
                                    qreal x3 = r.x() + r.width() * .5;
                                    qreal y0 = p1.y() + (x3 - p1.x()) * d;
                                    qreal c  = y0 - y3;
                                    if (c > 0) {
                                          p1.ry() -= c;
                                          p2.ry() -= c;
                                          }
                                    }
                              }
                        }
                  }
            }
      else {
            p1 = cr1->abbox().bottomLeft();
            p1.ry() += headDistance;

            if (cr1->type() == CHORD) {
                  const Chord* chord1 = static_cast<const Chord*>(cr1);
                  Stem* stem = chord1->stem();
                  if (stem && !chord1->up()) {
                        p1.setY(stem->abbox().bottom());
                        if (chord1->beam())
                              p1.setX(stem->abbox().x());
                        }
                  else if ((cr2->type() == CHORD) && stem && chord1->up()) {
                        const Chord* chord2 = static_cast<const Chord*>(cr2);
                        Stem* stem2 = chord2->stem();
                        if (stem2) {
                              int l1 = chord1->upNote()->line();
                              int l2 = chord2->upNote()->line();
                              p1.ry() = stem2->abbox().bottom() + _spatium * .5 * (l1 - l2);
                              }
                        }
                  }

            p2 = cr2->abbox().bottomRight();
            p2.ry() += headDistance;

            if (cr2->type() == CHORD) {
                  const Chord* chord2 = static_cast<const Chord*>(cr2);
                  Stem* stem = chord2->stem();
                  if (stem && !chord2->up()) {
                        if (chord2->beam())
                              p2.setX(stem->abbox().x());
                        p2.setY(stem->abbox().bottom());
                        }
                  else if ((cr1->type() == CHORD) && stem && chord2->up()) {
                        const Chord* chord1 = static_cast<const Chord*>(cr1);
                        int l1 = chord1->upNote()->line();
                        int l2 = chord2->upNote()->line();
                        p2.ry() = p1.ry() + _spatium * .5 * (l2 - l1);
                        }
                  }
            if (cr1->type() != CHORD && cr2->type() == CHORD) {
                  if (p2.y() > p1.y())
                        p1.setY(p2.y());
                  else
                        p2.setY(p1.y());
                  }
            else if (cr1->type() == CHORD && cr2->type() != CHORD) {
                  if (p1.y() > p2.y())
                        p2.setY(p1.y());
                  else
                        p1.setY(p2.y());
                  }

            // check for collisions

            int n = _elements.size();
            if (n >= 3) {
                  qreal d  = (p2.y() - p1.y())/(p2.x() - p1.x());
                  for (int i = 1; i < (n-1); ++i) {
                        Element* e = _elements[i];
                        if (e->type() == CHORD) {
                              const Chord* chord = static_cast<const Chord*>(e);
                              const Stem* stem = chord->stem();
                              if (stem) {
                                    QRectF r(chord->up() ? chord->abbox() : stem->abbox());
                                    qreal y3 = r.bottom();
                                    qreal x3 = r.x() + r.width() * .5;
                                    qreal y0 = p1.y() + (x3 - p1.x()) * d;
                                    qreal c  = y0 - y3;
                                    if (c < 0) {
                                          p1.ry() -= c;
                                          p2.ry() -= c;
                                          }
                                    }
                              }
                        }
                  }
            }

      qreal l1 = _spatium;          // bracket tip height
      qreal l2 = _spatium * .5;     // bracket distance to note

      setPos(0.0, 0.0);
      QPointF mp(parent()->pagePos());
      p1 -= mp;
      p2 -= mp;

      p1 += _p1;
      p2 += _p2;

      // center number
      qreal x3 = 0.0;
      qreal y3 = 0.0;
      qreal numberWidth = 0.0;
      if (_number) {
            _number->layout();
            x3 = p1.x() + (p2.x() - p1.x()) * .5;

            y3 = p1.y() + (p2.y() - p1.y()) * .5
//               - _number->bbox().height() * .5
               - (l1 + l2) * (_isUp ? 1.0 : -1.0);

            numberWidth = _number->bbox().width();
            _number->setPos(QPointF(x3, y3) - ipos());
            }

      if (_hasBracket) {
            qreal slope = (p2.y() - p1.y()) / (p2.x() - p1.x());

            if (_isUp) {
                  if (_number) {
                        bracketL[0] = QPointF(p1.x(), p1.y() - l2);
                        bracketL[1] = QPointF(p1.x(), p1.y() - l1 - l2);
                        qreal x     = x3 - numberWidth * .5 - _spatium * .5;
                        qreal y     = p1.y() + (x - p1.x()) * slope;
                        bracketL[2] = QPointF(x,   y - l1 - l2);

                        x           = x3 + numberWidth * .5 + _spatium * .5;
                        y           = p1.y() + (x - p1.x()) * slope;
                        bracketR[0] = QPointF(x,   y - l1 - l2);
                        bracketR[1] = QPointF(p2.x(), p2.y() - l1 - l2);
                        bracketR[2] = QPointF(p2.x(), p2.y() - l2);
                        }
                  else {
                        bracketL[0] = QPointF(p1.x(), p1.y() - l2);
                        bracketL[1] = QPointF(p1.x(), p1.y() - l1 - l2);
                        bracketL[2] = QPointF(p2.x(), p2.y() - l1 - l2);
                        bracketL[3] = QPointF(p2.x(), p2.y() - l2);
                        }
                  }
            else {
                  if (_number) {
                        bracketL[0] = QPointF(p1.x(), p1.y() + l2);
                        bracketL[1] = QPointF(p1.x(), p1.y() + l1 + l2);
                        qreal x     = x3 - numberWidth * .5 - _spatium * .5;
                        qreal y     = p1.y() + (x - p1.x()) * slope;
                        bracketL[2] = QPointF(x,   y + l1 + l2);

                        x           = x3 + numberWidth * .5 + _spatium * .5;
                        y           = p1.y() + (x - p1.x()) * slope;
                        bracketR[0] = QPointF(x,   y + l1 + l2);
                        bracketR[1] = QPointF(p2.x(), p2.y() + l1 + l2);
                        bracketR[2] = QPointF(p2.x(), p2.y() + l2);
                        }
                  else {
                        bracketL[0] = QPointF(p1.x(), p1.y() + l2);
                        bracketL[1] = QPointF(p1.x(), p1.y() + l1 + l2);
                        bracketL[2] = QPointF(p2.x(), p2.y() + l1 + l2);
                        bracketL[3] = QPointF(p2.x(), p2.y() + l2);
                        }
                  }
            }
      QRectF r;
      if (_number) {
            r |= _number->bbox().translated(_number->pos());
            if (_hasBracket) {
                  QRectF b;
                  b.setCoords(bracketL[1].x(), bracketL[1].y(), bracketR[2].x(), bracketR[2].y());
                  r |= b;
                  }
            }
      else if (_hasBracket) {
            QRectF b;
            b.setCoords(bracketL[1].x(), bracketL[1].y(), bracketL[3].x(), bracketL[3].y());
            r |= b;
            }
      setbbox(r);
      }

//---------------------------------------------------------
//   draw
//---------------------------------------------------------

void Tuplet::draw(QPainter* painter) const
      {
      // if in a TAB without stems, tuplets are not shown
      if (staff() && staff()->isTabStaff() && staff()->staffType()->slashStyle())
            return;

      QColor color(curColor());
      if (_number) {
            painter->setPen(color);
            QPointF pos(_number->pos());
            painter->translate(pos);
            _number->draw(painter);
            painter->translate(-pos);
            }
      if (_hasBracket) {
            painter->setPen(QPen(color, spatium() * .1));
            if (!_number)
                  painter->drawPolyline(bracketL, 4);
            else {
                  painter->drawPolyline(bracketL, 3);
                  painter->drawPolyline(bracketR, 3);
                  }
            }
      }

//---------------------------------------------------------
//   write
//---------------------------------------------------------

void Tuplet::write(Xml& xml) const
      {
      xml.stag(QString("Tuplet id=\"%1\"").arg(_id));
      if (tuplet())
            xml.tag("Tuplet", tuplet()->id());
      Element::writeProperties(xml);

      writeProperty(xml, P_DIRECTION);
      writeProperty(xml, P_NUMBER_TYPE);
      writeProperty(xml, P_BRACKET_TYPE);
      writeProperty(xml, P_NORMAL_NOTES);
      writeProperty(xml, P_ACTUAL_NOTES);
      writeProperty(xml, P_P1);
      writeProperty(xml, P_P2);

      xml.tag("baseNote", _baseLen.name());

      if (_number) {
            xml.stag("Number");
            _number->writeProperties(xml);
            xml.etag();
            }
      if (!userOff().isNull())
            xml.tag("offset", userOff() / spatium());
      xml.etag();
      }

//---------------------------------------------------------
//   read
//---------------------------------------------------------

void Tuplet::read(XmlReader& e)
      {
      int bl = -1;
      _id    = e.intAttribute("id", 0);

      while (e.readNextStartElement()) {
            const QStringRef& tag(e.name());

            if (tag == "direction")
                  _direction = MScore::Direction(e.readInt());
            else if (tag == "numberType")
                  _numberType = e.readInt();
            else if (tag == "bracketType")
                  _bracketType = e.readInt();
            else if (tag == "normalNotes")
                  _ratio.setDenominator(e.readInt());
            else if (tag == "actualNotes")
                  _ratio.setNumerator(e.readInt());
            else if (tag == "p1")
                  _p1 = e.readPoint();
            else if (tag == "p2")
                  _p2 = e.readPoint();
            else if (tag == "baseNote")
                  _baseLen = TDuration(e.readElementText());
            else if (tag == "Number") {
                  _number = new Text(score());
                  _number->setParent(this);
                  _number->read(e);
                  _number->setTextStyleType(TEXT_STYLE_TUPLET);
                  _number->setVisible(visible());     //?? override saved property
                  }
            else if (tag == "subtype")    // obsolete
                  ;
            else if (tag == "hasNumber")             // obsolete
                  _numberType = e.readInt() ? SHOW_NUMBER : NO_TEXT;
            else if (tag == "hasLine") {          // obsolete
                  _hasBracket = e.readInt();
                  _bracketType = AUTO_BRACKET;
                  }
            else if (tag == "baseLen")            // obsolete
                  bl = e.readInt();
            else if (!DurationElement::readProperties(e))
                  e.unknown();
            }
      Fraction f(_ratio.denominator(), _baseLen.fraction().denominator());
      setDuration(f);
      if (bl != -1) {         // obsolete
            TDuration d;
            d.setVal(bl);
            _baseLen = d;
// qDebug("Tuplet base len %d/%d", d.fraction().numerator(), d.fraction().denominator());
// qDebug("   %s  dots %d, %d/%d", qPrintable(d.name()), d.dots(), _ratio.numerator(), _ratio.denominator());
            d.setVal(bl * _ratio.denominator());
            setDuration(d.fraction());
            }
      }

//---------------------------------------------------------
//   add
//---------------------------------------------------------

void Tuplet::add(Element* e)
      {
#ifndef NDEBUG
      foreach(DurationElement* el, _elements) {
            if (el == e) {
                  qDebug("Tuplet::add: %p %s already there", e, e->name());
                  abort();
                  }
            }
#endif

      switch(e->type()) {
            case TEXT:
                  _number = static_cast<Text*>(e);
                  break;
            case CHORD:
            case REST:
            case TUPLET: {
                  bool found = false;
                  DurationElement* de = static_cast<DurationElement*>(e);
                  int tick = de->tick();
                  if (tick != -1) {
                        for (int i = 0; i < _elements.size(); ++i) {
                              if (_elements[i]->tick() > tick) {
                                    _elements.insert(i, de);
                                    found = true;
                                    break;
                                    }
                              }
                        }
                  if (!found)
                        _elements.append(de);
                  de->setTuplet(this);

                  // the tick position of a tuplet is the tick position of its
                  // first element:
                  setTick(_elements.front()->tick());
                  }
                  break;

            default:
                  qDebug("Tuplet::add() unknown element");
                  break;
            }
      }

//---------------------------------------------------------
//   remove
//---------------------------------------------------------

void Tuplet::remove(Element* e)
      {
      switch(e->type()) {
            case TEXT:
                  if (e == _number)
                        _number = 0;
                  break;
            case CHORD:
            case REST:
            case TUPLET:
                  if (!_elements.removeOne(static_cast<DurationElement*>(e))) {
                        qDebug("Tuplet::remove: cannot find element");
                        qDebug("  elements %d", _elements.size());
                        }
                  break;
            default:
                  qDebug("Tuplet::remove: unknown element");
                  break;
            }
      }

//---------------------------------------------------------
//   isEditable
//---------------------------------------------------------

bool Tuplet::isEditable() const
      {
      return _hasBracket;
      }

//---------------------------------------------------------
//   editDrag
//---------------------------------------------------------

void Tuplet::editDrag(const EditData& ed)
      {
      if (ed.curGrip == 0)
            _p1 += ed.delta;
      else
            _p2 += ed.delta;
      setGenerated(false);
      layout();
      score()->setUpdateAll(true);
      }

//---------------------------------------------------------
//   updateGrips
//---------------------------------------------------------

void Tuplet::updateGrips(int* grips, QRectF*grip) const
      {
      *grips = 2;
      grip[0].translate(pagePos() + p1);
      grip[1].translate(pagePos() + p2);
      }

//---------------------------------------------------------
//   reset
//---------------------------------------------------------

void Tuplet::reset()
      {
      score()->addRefresh(canvasBoundingRect());

      undoChangeProperty(P_P1,        QPointF());
      undoChangeProperty(P_P2,        QPointF());
      undoChangeProperty(P_DIRECTION, propertyDefault(P_DIRECTION));

      Element::reset();
      layout();
      score()->addRefresh(canvasBoundingRect());
      }

//---------------------------------------------------------
//   dump
//---------------------------------------------------------

void Tuplet::dump() const
      {
      Element::dump();
      qDebug("ratio %s", qPrintable(_ratio.print()));
      }

//---------------------------------------------------------
//   setTrack
//---------------------------------------------------------

void Tuplet::setTrack(int val)
      {
      Element::setTrack(val);
      }

//---------------------------------------------------------
//   tickGreater
//---------------------------------------------------------

static bool tickGreater(const DurationElement* a, const DurationElement* b)
      {
      return a->tick() < b->tick();
      }

//---------------------------------------------------------
//   sortElements
//---------------------------------------------------------

void Tuplet::sortElements()
      {
      qSort(_elements.begin(), _elements.end(), tickGreater);
      }

//---------------------------------------------------------
//   getProperty
//---------------------------------------------------------

QVariant Tuplet::getProperty(P_ID propertyId) const
      {
      switch(propertyId) {
            case P_DIRECTION:
                  return _direction;
            case P_NUMBER_TYPE:
                  return _numberType;
            case P_BRACKET_TYPE:
                  return _bracketType;
            case P_NORMAL_NOTES:
                  return _ratio.denominator();
            case P_ACTUAL_NOTES:
                  return _ratio.numerator();
            case P_P1:
                  return _p1;
            case P_P2:
                  return _p2;
            default:
                  break;
            }
      return DurationElement::getProperty(propertyId);
      }

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

bool Tuplet::setProperty(P_ID propertyId, const QVariant& v)
      {
      score()->addRefresh(canvasBoundingRect());
      switch(propertyId) {
            case P_DIRECTION:
                  setDirection(MScore::Direction(v.toInt()));
                  break;
            case P_NUMBER_TYPE:
                  setNumberType(v.toInt());
                  break;
            case P_BRACKET_TYPE:
                  setBracketType(v.toInt());
                  break;
            case P_NORMAL_NOTES:
                  _ratio.setDenominator(v.toInt());
                  break;
            case P_ACTUAL_NOTES:
                  _ratio.setNumerator(v.toInt());
                  break;
            case P_P1:
                  _p1 = v.toPointF();
                  break;
            case P_P2:
                  _p2 = v.toPointF();
                  break;
            default:
                  return DurationElement::setProperty(propertyId, v);
                  break;
            }
      score()->setLayoutAll(true);
      return true;
      }

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

QVariant Tuplet::propertyDefault(P_ID id) const
      {
      switch(id) {
            case P_DIRECTION:
                  return MScore::AUTO;
            case P_NUMBER_TYPE:
                  return Tuplet::SHOW_NUMBER;
            case P_BRACKET_TYPE:
                  return Tuplet::AUTO_BRACKET;
            case P_NORMAL_NOTES:
            case P_ACTUAL_NOTES:
                  return 1;
            case P_P1:
            case P_P2:
                  return QPointF();
            default:
                  return DurationElement::propertyDefault(id);
            }
      }

}

