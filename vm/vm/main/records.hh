// Copyright © 2011, Université catholique de Louvain
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// *  Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
// *  Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#ifndef __RECORDS_H
#define __RECORDS_H

#include "mozartcore.hh"

#ifndef MOZART_GENERATOR

namespace mozart {

////////////////
// BaseRecord //
////////////////

template <class T>
StableNode* BaseRecord<T>::getElement(Self self, size_t index) {
  return &self[index];
}

template <class T>
OpResult BaseRecord<T>::width(Self self, VM vm, size_t& result) {
  result = getWidth();
  return OpResult::proceed();
}

template <class T>
OpResult BaseRecord<T>::arityList(Self self, VM vm,
                                  UnstableNode& result) {
  UnstableNode res = build(vm, vm->coreatoms.nil);

  for (size_t i = getWidth(); i > 0; i--) {
    UnstableNode feature;
    static_cast<T*>(this)->getFeatureAt(self, vm, i-1, feature);

    UnstableNode temp = buildCons(vm, std::move(feature), std::move(res));
    res = std::move(temp);
  }

  result = std::move(res);
  return OpResult::proceed();
}

template <class T>
OpResult BaseRecord<T>::initElement(Self self, VM vm,
                                    size_t index, RichNode value) {
  self[index].init(vm, value);
  return OpResult::proceed();
}

template <class T>
OpResult BaseRecord<T>::waitOr(Self self, VM vm,
                               UnstableNode& result) {
  // If there is a field which is bound, then return its feature
  for (size_t i = 0; i < getArraySize(); i++) {
    RichNode element = self[i];
    if (!element.isTransient()) {
      static_cast<T*>(this)->getFeatureAt(self, vm, i, result);
      return OpResult::proceed();
    } else if (element.is<FailedValue>()) {
      return OpResult::waitFor(vm, element);
    }
  }

  // Create the control variable
  UnstableNode unstableControlVar = Variable::build(vm);
  RichNode controlVar = unstableControlVar;
  controlVar.ensureStable(vm);

  // Add the control variable to the suspension list of all the fields
  for (size_t i = 0; i < getArraySize(); i++) {
    DataflowVariable(self[i]).addToSuspendList(vm, controlVar);
  }

  // Wait for the control variable
  return OpResult::waitFor(vm, controlVar);
}

///////////
// Tuple //
///////////

#include "Tuple-implem.hh"

Tuple::Tuple(VM vm, size_t width, StaticArray<StableNode> _elements,
             RichNode label) {
  _label.init(vm, label);
  _width = width;

  // Initialize elements with non-random data
  // TODO An Uninitialized type?
  for (size_t i = 0; i < width; i++)
    _elements[i].init(vm);
}

Tuple::Tuple(VM vm, size_t width, StaticArray<StableNode> _elements,
             GR gr, Self from) {
  _width = width;
  gr->copyStableNode(_label, from->_label);

  for (size_t i = 0; i < width; i++)
    gr->copyStableNode(_elements[i], from[i]);
}

StaticArray<StableNode> Tuple::getElementsArray(Self self) {
  return self.getArray();
}

bool Tuple::equals(Self self, VM vm, Self right, WalkStack& stack) {
  if (_width != right->_width)
    return false;

  stack.pushArray(vm, self.getArray(), right.getArray(), _width);
  stack.push(vm, &_label, &right->_label);

  return true;
}

void Tuple::getValueAt(Self self, VM vm, nativeint feature,
                       UnstableNode& result) {
  result.copy(vm, self[(size_t) feature - 1]);
}

void Tuple::getFeatureAt(Self self, VM vm, size_t index,
                         UnstableNode& result) {
  result = SmallInt::build(vm, index+1);
}

OpResult Tuple::label(Self self, VM vm, UnstableNode& result) {
  result.copy(vm, _label);
  return OpResult::proceed();
}

OpResult Tuple::clone(Self self, VM vm, UnstableNode& result) {
  result = Tuple::build(vm, _width, _label);

  auto tuple = RichNode(result).as<Tuple>();
  for (size_t i = 0; i < _width; i++)
    tuple.getElement(i)->init(vm, OptVar::build(vm));

  return OpResult::proceed();
}

OpResult Tuple::testRecord(Self self, VM vm, RichNode arity, bool& result) {
  result = false;
  return OpResult::proceed();
}

OpResult Tuple::testTuple(Self self, VM vm, RichNode label, size_t width,
                          bool& result) {
  if (width == _width) {
    return mozart::equals(vm, _label, label, result);
  } else {
    result = false;
    return OpResult::proceed();
  }
}

OpResult Tuple::testLabel(Self self, VM vm, RichNode label, bool& result) {
  return mozart::equals(vm, _label, label, result);
}

void Tuple::printReprToStream(Self self, VM vm, std::ostream& out,
                              int depth) {
  out << repr(vm, _label, depth) << "(";

  if (depth <= 1) {
    out << "...";
  } else {
    for (size_t i = 0; i < _width; i++) {
      if (i > 0)
        out << " ";
      out << repr(vm, self[i], depth);
    }
  }

  out << ")";
}

OpResult Tuple::isVirtualString(Self self, VM vm, bool& result) {
  result = false;
  if (hasSharpLabel(vm)) {
    for (size_t i = 0; i < _width; ++ i) {
      MOZART_CHECK_OPRESULT(VirtualString(self[i]).isVirtualString(vm, result));
      if (!result)
        return OpResult::proceed();
    }
    result = true;
  }
  return OpResult::proceed();
}

OpResult Tuple::toString(Self self, VM vm, std::basic_ostream<nchar>& sink) {
  if (!hasSharpLabel(vm))
    return raiseTypeError(vm, MOZART_STR("VirtualString"), self);

  for (size_t i = 0; i < _width; ++ i) {
    MOZART_CHECK_OPRESULT(VirtualString(self[i]).toString(vm, sink));
  }

  return OpResult::proceed();
}

OpResult Tuple::vsLength(Self self, VM vm, nativeint& result) {
  if (!hasSharpLabel(vm))
    return raiseTypeError(vm, MOZART_STR("VirtualString"), self);

  result = 0;
  for (size_t i = 0; i < _width; ++ i) {
    nativeint thisLength = 0;
    UnstableNode tempNode (vm, self[i]);
    MOZART_CHECK_OPRESULT(VirtualString(tempNode).vsLength(vm, thisLength));
    result += thisLength;
  }

  return OpResult::proceed();
}

bool Tuple::hasSharpLabel(VM vm) {
  RichNode label = _label;
  return label.is<Atom>() && label.as<Atom>().value() == vm->coreatoms.sharp;
}

//////////
// Cons //
//////////

#include "Cons-implem.hh"

Cons::Cons(VM vm, RichNode head, RichNode tail) {
  _elements[0].init(vm, head);
  _elements[1].init(vm, tail);
}

Cons::Cons(VM vm) {
  _elements[0].init(vm);
  _elements[1].init(vm);
}

Cons::Cons(VM vm, GR gr, Self from) {
  gr->copyStableNode(_elements[0], from->_elements[0]);
  gr->copyStableNode(_elements[1], from->_elements[1]);
}

bool Cons::equals(Self self, VM vm, Self right, WalkStack& stack) {
  stack.push(vm, &_elements[1], &right->_elements[1]);
  stack.push(vm, &_elements[0], &right->_elements[0]);

  return true;
}

void Cons::getValueAt(Self self, VM vm, nativeint feature,
                      UnstableNode& result) {
  result.copy(vm, _elements[feature-1]);
}

OpResult Cons::label(Self self, VM vm, UnstableNode& result) {
  result = Atom::build(vm, vm->coreatoms.pipe);
  return OpResult::proceed();
}

OpResult Cons::width(Self self, VM vm, size_t& result) {
  result = 2;
  return OpResult::proceed();
}

OpResult Cons::arityList(Self self, VM vm, UnstableNode& result) {
  result = buildList(vm, 1, 2);
  return OpResult::proceed();
}

OpResult Cons::clone(Self self, VM vm, UnstableNode& result) {
  result = buildCons(vm, OptVar::build(vm), OptVar::build(vm));
  return OpResult::proceed();
}

OpResult Cons::waitOr(Self self, VM vm, UnstableNode& result) {
  RichNode head = _elements[0];
  RichNode tail = _elements[1];

  // If there is a field which is bound, then return its feature
  if (!head.isTransient()) {
    result = SmallInt::build(vm, 1);
    return OpResult::proceed();
  } else if (!tail.isTransient()) {
    result = SmallInt::build(vm, 2);
    return OpResult::proceed();
  }

  // If there is a feature which is a failed value, wait for it
  if (head.is<FailedValue>())
    return OpResult::waitFor(vm, head);
  else if (tail.is<FailedValue>())
    return OpResult::waitFor(vm, tail);

  // Create the control variable
  UnstableNode unstableControlVar = Variable::build(vm);
  RichNode controlVar = unstableControlVar;
  controlVar.ensureStable(vm);

  // Add the control variable to the suspension list of both fields
  DataflowVariable(head).addToSuspendList(vm, controlVar);
  DataflowVariable(tail).addToSuspendList(vm, controlVar);

  // Wait for the control variable
  return OpResult::waitFor(vm, controlVar);
}

OpResult Cons::testRecord(Self self, VM vm, RichNode arity, bool& result) {
  result = false;
  return OpResult::proceed();
}

OpResult Cons::testTuple(Self self, VM vm, RichNode label, size_t width,
                         bool& result) {
  result = (width == 2) && label.is<Atom>() &&
    (label.as<Atom>().value() == vm->coreatoms.pipe);
  return OpResult::proceed();
}

OpResult Cons::testLabel(Self self, VM vm, RichNode label, bool& result) {
  result = label.is<Atom>() && (label.as<Atom>().value() == vm->coreatoms.pipe);
  return OpResult::proceed();
}

namespace internal {

template <class F>
inline
OpResult withConsAsVirtualString(VM vm, RichNode cons, const F& onChar) {
  return ozListForEach(vm, cons,
    [&, vm](nativeint c) -> OpResult {
      if (c < 0 || c >= 256) {
        UnstableNode errNode = SmallInt::build(vm, c);
        return raiseTypeError(vm, MOZART_STR("char"), errNode);
      }
      onChar((char32_t) c);
      return OpResult::proceed();
    },
    MOZART_STR("VirtualString")
  );
}

}

OpResult Cons::isVirtualString(Self self, VM vm, bool& result) {
  OpResult res = internal::withConsAsVirtualString(vm, self, [](char32_t){});
  if (res.isProceed()) {
    result = true;
    return res;
  } else if (res.kind() == OpResult::orRaise) {
    result = false;
    return OpResult::proceed();
  } else {
    return res;
  }
}

OpResult Cons::toString(Self self, VM vm, std::basic_ostream<nchar>& sink) {
  MOZART_CHECK_OPRESULT(internal::withConsAsVirtualString(vm, self,
    [&](char32_t c) {
      nchar buffer[4];
      nativeint length = toUTF(c, buffer);
      sink.write(buffer, length);
    }
  ));

  return OpResult::proceed();
}

OpResult Cons::vsLength(Self self, VM vm, nativeint& result) {
  nativeint length = 0;

  MOZART_CHECK_OPRESULT(internal::withConsAsVirtualString(vm, self,
    [&](char32_t) { ++ length; }
  ));

  result = length;
  return OpResult::proceed();
}

void Cons::printReprToStream(Self self, VM vm, std::ostream& out, int depth) {
  out << repr(vm, _elements[0], depth) << "|" << repr(vm, _elements[1], depth);
}

///////////
// Arity //
///////////

#include "Arity-implem.hh"

Arity::Arity(VM vm, size_t width, StaticArray<StableNode> _elements,
             RichNode label) {
  _label.init(vm, label);
  _width = width;

  // Initialize elements with non-random data
  // TODO An Uninitialized type?
  for (size_t i = 0; i < width; i++)
    _elements[i].init(vm);
}

Arity::Arity(VM vm, size_t width, StaticArray<StableNode> _elements,
             GR gr, Self from) {
  _width = width;
  gr->copyStableNode(_label, from->_label);

  for (size_t i = 0; i < width; i++)
    gr->copyStableNode(_elements[i], from[i]);
}

StableNode* Arity::getElement(Self self, size_t index) {
  return &self[index];
}

bool Arity::equals(Self self, VM vm, Self right, WalkStack& stack) {
  if (_width != right->_width)
    return false;

  stack.pushArray(vm, self.getArray(), right.getArray(), _width);
  stack.push(vm, &_label, &right->_label);

  return true;
}

OpResult Arity::initElement(Self self, VM vm, size_t index, RichNode value) {
  self[index].init(vm, value);
  return OpResult::proceed();
}

OpResult Arity::lookupFeature(Self self, VM vm, RichNode feature,
                              bool& found, size_t& index) {
  MOZART_REQUIRE_FEATURE(feature);

  // Dichotomic search
  size_t lo = 0;
  size_t hi = getWidth();

  while (lo < hi) {
    size_t mid = (lo + hi) / 2; // no need to worry about overflow, here
    int comparison = compareFeatures(vm, feature, self[mid]);

    if (comparison == 0) {
      found = true;
      index = mid;
      return OpResult::proceed();
    } else if (comparison < 0) {
      hi = mid;
    } else {
      lo = mid+1;
    }
  }

  found = false;
  return OpResult::proceed();
}

void Arity::printReprToStream(Self self, VM vm, std::ostream& out,
                              int depth) {
  out << "<Arity " << repr(vm, _label, depth) << "(";

  if (depth <= 1) {
    out << "...";
  } else {
    for (size_t i = 0; i < _width; i++) {
      if (i > 0)
        out << " ";
      out << repr(vm, self[i], depth);
    }
  }

  out << ")>";
}

////////////
// Record //
////////////

#include "Record-implem.hh"

Record::Record(VM vm, size_t width, StaticArray<StableNode> _elements,
               RichNode arity) {
  assert(arity.is<Arity>());

  _arity.init(vm, arity);
  _width = width;

  // Initialize elements with non-random data
  // TODO An Uninitialized type?
  for (size_t i = 0; i < width; i++)
    _elements[i].init(vm);
}

Record::Record(VM vm, size_t width, StaticArray<StableNode> _elements,
               GR gr, Self from) {
  gr->copyStableNode(_arity, from->_arity);
  _width = width;

  for (size_t i = 0; i < width; i++)
    gr->copyStableNode(_elements[i], from[i]);
}

StaticArray<StableNode> Record::getElementsArray(Self self) {
  return self.getArray();
}

bool Record::equals(Self self, VM vm, Self right, WalkStack& stack) {
  if (_width != right->_width)
    return false;

  stack.pushArray(vm, self.getArray(), right.getArray(), _width);
  stack.push(vm, &_arity, &right->_arity);

  return true;
}

void Record::getFeatureAt(Self self, VM vm, size_t index,
                          UnstableNode& result) {
  result.copy(vm, *RichNode(_arity).as<Arity>().getElement(index));
}

OpResult Record::lookupFeature(Self self, VM vm, RichNode feature,
                               bool& found, nullable<UnstableNode&> value) {
  size_t index = 0;
  MOZART_CHECK_OPRESULT(
    RichNode(_arity).as<Arity>().lookupFeature(vm, feature, found, index));

  if (found && value.isDefined())
    value.get().copy(vm, self[index]);

  return OpResult::proceed();
}

OpResult Record::lookupFeature(Self self, VM vm, nativeint feature,
                               bool& found, nullable<UnstableNode&> value) {
  UnstableNode featureNode = mozart::build(vm, feature);
  return lookupFeature(self, vm, featureNode, found, value);
}

OpResult Record::label(Self self, VM vm, UnstableNode& result) {
  result.copy(vm, *RichNode(_arity).as<Arity>().getLabel());
  return OpResult::proceed();
}

OpResult Record::clone(Self self, VM vm, UnstableNode& result) {
  result = Record::build(vm, _width, _arity);

  auto record = RichNode(result).as<Record>();
  for (size_t i = 0; i < _width; i++)
    record.getElement(i)->init(vm, OptVar::build(vm));

  return OpResult::proceed();
}

OpResult Record::testRecord(Self self, VM vm, RichNode arity, bool& result) {
  return mozart::equals(vm, _arity, arity, result);
}

OpResult Record::testTuple(Self self, VM vm, RichNode label, size_t width,
                           bool& result) {
  result = false;
  return OpResult::proceed();
}

OpResult Record::testLabel(Self self, VM vm, RichNode label, bool& result) {
  return mozart::equals(
    vm, *RichNode(_arity).as<Arity>().getLabel(), label, result);
}

void Record::printReprToStream(Self self, VM vm, std::ostream& out,
                               int depth) {
  UnstableNode label;
  MOZART_ASSERT_PROCEED(this->label(self, vm, label));

  out << repr(vm, label, depth) << "(";

  if (depth <= 1) {
    out << "...";
  } else {
    for (size_t i = 0; i < _width; i++) {
      if (i > 0)
        out << " ";

      UnstableNode feature;
      getFeatureAt(self, vm, i, feature);

      out << repr(vm, feature, depth) << ":" << repr(vm, self[i], depth);
    }
  }

  out << ")";
}

///////////
// Chunk //
///////////

#include "Chunk-implem.hh"

void Chunk::create(StableNode*& self, VM vm, GR gr, Self from) {
  gr->copyStableRef(self, from.get().getUnderlying());
}

OpResult Chunk::lookupFeature(Self self, VM vm, RichNode feature,
                              bool& found, nullable<UnstableNode&> value) {
  return Dottable(*_underlying).lookupFeature(vm, feature, found, value);
}

OpResult Chunk::lookupFeature(Self self, VM vm, nativeint feature,
                              bool& found, nullable<UnstableNode&> value) {
  return Dottable(*_underlying).lookupFeature(vm, feature, found, value);
}

}

#endif // MOZART_GENERATOR

#endif // __RECORDS_H
