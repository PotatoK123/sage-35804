/** @file numeric.cpp
 *
 *  This file contains the interface to the underlying bignum package.
 *  Its most important design principle is to completely hide the inner
 *  working of that other package from the user of GiNaC.  It must either 
 *  provide implementation of arithmetic operators and numerical evaluation
 *  of special functions or implement the interface to the bignum package. */

/*
 *  This is a modified version of code included with Ginac.  
 *  The modifications and modified version is:
 * 
 *      GiNaC-SAGE Copyright (C) 2008 William Stein
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */


/*  The original copyright:
 * 
 *  GiNaC Copyright (C) 1999-2008 Johannes Gutenberg University Mainz, Germany
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"

#include <vector>
#include <stdexcept>
#include <string>
#include <sstream>
#include <limits>

namespace math {
#include <math.h>
}

#include "numeric.h"
#include "ex.h"
#include "operators.h"
#include "archive.h"
#include "tostring.h"
#include "utils.h"

//#define DEBUG
//#define VERBOSE

#ifdef DEBUG
#define todo(s) std::cerr << "TODO: " << s << std::endl;
#define stub(s) std::cerr << "Hit STUB: " << s << std::endl;
#define fake(s) std::cerr << "fake: " << s << std::endl;
#define ASSERT(s, msg) if (!s) { std::cerr << "Failed assertion: " << msg << std::endl; }
#else
#define todo(s)
#define stub(s)
#define fake(s)
#endif

#ifdef VERBOSE
#define verbose(s) std::cerr << s << std::endl;
#define verbose2(s,t) std::cerr << s << " " << t << std::endl;
#define verbose3(s,t,u) std::cerr << s << " " << t << ", " << u << std::endl;
#else
#define verbose(s)
#define verbose2(s,t)
#define verbose3(s,t,u)
#endif


//////////////////////////////////////////////////////////////
// Python Interface
//////////////////////////////////////////////////////////////

void ginac_error(const char* s) {
  std::cerr << s << std::endl;
  abort();
}



void py_error(const char* s) {
  #ifdef DEBUG
    std::cerr << "PYTHON ERROR! " << s << std::endl;
  #endif
  if (PyErr_Occurred()) {
    PyErr_Print();
    PyErr_Clear();
    abort();
  }
}

static PyObject* pyfunc_Integer = 0;
void ginac_pyinit_Integer(PyObject* f) {
  Py_INCREF(f);
  pyfunc_Integer = f;
}

static PyObject* pyfunc_Float = 0;
void ginac_pyinit_Float(PyObject* f) {
  Py_INCREF(f);
  pyfunc_Float = f;
}

static PyObject* pyfunc_gcd = 0;
void ginac_pyinit_gcd(PyObject* f) {
  Py_INCREF(f);
  pyfunc_gcd = f;
}

static PyObject* pyfunc_lcm = 0;
void ginac_pyinit_lcm(PyObject* f) {
  Py_INCREF(f);
  pyfunc_lcm = f;
}

static PyObject* pyfunc_binomial = 0;
void ginac_pyinit_binomial(PyObject* f) {
  Py_INCREF(f);
  pyfunc_binomial = f;
}



namespace GiNaC {

long 
gcd_long ( long a, long b )
{
  long c;
  while ( a != 0 ) {
     c = a; a = b%a;  b = c;
  }
  return b;
}

/* returns ln(Gamma(xx)) for xx > 0 */
double gammln(double xx)
{
   double x,y,tmp,ser;
   static double cof[6]={76.18009172947146,-86.50532032941677,
      24.01409824083091,-1.231739572450155,0.1208650973866179e-2,
      -0.5395239384953e-5};
   int j;

   y=x=xx;
   tmp=x+5.5;
   tmp -= (x+0.5)*math::log(tmp);
   ser = 1.000000000190015;
   for (j=0;j<=5;j++) ser += cof[j]/++y;
   return -tmp + math::log(2.5066282746310005*ser/x);
}

/* returns ln(n!) */
double factln(int n)
{
   static double a[256];

   if (n<0) 
   {
      printf("binomial: factln: negative factorial (%d) attempted.\n",n);
   }
   if (n<=1) return 0.0;
   /* check if in range of table */
   if (n<=255) return a[n] ? a[n] : (a[n] = gammln(n+1.0));
   /* out of range of table */
   else return gammln(n+1.0);
}

/* returns n choose k, as a double */
double binomial(int n, int k)
{
  return math::floor(0.5 + math::exp(factln(n)-factln(k)-factln(n-k)));
}

  ///////////////////////////////////////////////////////////////////////////////
  // class Number_T
  ///////////////////////////////////////////////////////////////////////////////

  ///////////////////////////////////////////////////////////////////////////////
  // class Number_T
  ///////////////////////////////////////////////////////////////////////////////

PyObject* ZERO = PyInt_FromLong(0);   // todo: never freed
PyObject* ONE  = PyInt_FromLong(1);   // todo: never freed
PyObject* TWO  = PyInt_FromLong(2);   // todo: never freed
PyObject* s_lcm = PyString_FromString("lcm");  // todo: never freed
PyObject* s_gcd = PyString_FromString("gcd");  // todo: never freed
PyObject* s_numerator = PyString_FromString("numerator");  // todo: never freed
PyObject* s_denominator = PyString_FromString("denominator");  // todo: never freed

  std::ostream& operator << (std::ostream& os, const Number_T& s) {
    switch(s.t) {
    case LONG:
      return os << s.v._long;
    case DOUBLE:
      return os << s.v._double;
    case PYOBJECT:
      PyObject* o = PyObject_Repr(s.v._pyobject);
      if (!o) {
	// TODO: get proper exception.
	os << "(exception printing python object)";
      } else {
	os << PyString_AsString(o);
	Py_DECREF(o);
      }
      return os;
    default:
      stub("operator << type not handled");
    }
  }

  PyObject* to_pyobject(const Number_T& x) {
    // Returns a New Reference
    PyObject* o;
    switch(x.t) {
      case LONG:
	if (!(o = PyInt_FromLong(x.v._long))) {
	  py_error("Error coercing a long to an Integer");
	}
	return o;
      case DOUBLE:
	if (!(o =  PyFloat_FromDouble(x.v._double)))
	  py_error("Error creating double");
	return o;
	//if (!(o = PyObject_CallFunction(pyfunc_Float, "d", x.v._double))) {
	//  py_error("Error coercing a long to an Integer");
      case PYOBJECT:
        Py_INCREF(x.v._pyobject);
        return x.v._pyobject;
      default:
	stub("to_pyobject -- not able to do conversion to pyobject; everything else will be nonsense");
	return 0;
      }
  }
  
  void coerce(Number_T& new_left, Number_T& new_right, const Number_T& left, const Number_T& right) {
    verbose("coerce");
    // Return a Number_T 
    if (left.t == right.t) {
      new_left = left;
      new_right = right;
      return;
    }
    PyObject* o;
    switch(left.t) {
    case LONG:
      switch(right.t) {
      case DOUBLE:
	new_left = (double)left;
	new_right = right;
	return;
      case PYOBJECT:
	verbose("About to coerce a Python long to an Integer");
	if (!(o = PyObject_CallFunction(pyfunc_Integer, "l", left.v._long))) {
	  py_error("Error coercing a long to an Integer");
	}
	new_left = o;
	Py_DECREF(o);
	//new_left = PyInt_FromLong(left);
	new_right = right;
	return;
      default:
	stub("** coerce not fully implemented yet -- left LONG**");
      }
    case DOUBLE:
      switch(right.t) {
      case LONG:
	new_left = left;
	new_right = (double) right;
	return;
      case PYOBJECT:
	new_left = PyFloat_FromDouble(left);
	new_right = right;
	return;
      default:
	stub("** coerce not fully implemented -- left DOUBLE ** ");
      }
    case PYOBJECT:
      new_right = to_pyobject(right);
      return;
    }
    stub("** coerce not fully implemented yet **");
  }


  Number_T pow(const Number_T& base, const Number_T& exp) {
    verbose("pow");
    if (base.t != exp.t) {
      Number_T a, b;
      coerce(a, b, base, exp);
      return pow(a,b);
    }
    switch (base.t) {
    case DOUBLE:
      return math::pow(base.v._double, exp.v._double);
    case LONG:
      // TODO: change to use GMP!
      return math::pow((double)base.v._long, (double)exp.v._long);
    case PYOBJECT:
      return PyNumber_Power(base.v._pyobject, exp.v._pyobject, Py_None);
    default:
      stub("pow Number_T");
    }
  }

  Number_T::Number_T()  { 
    verbose("Number_T::Number_T()");
    //t = LONG;
    //v._long = 0;

    t = PYOBJECT;
    if (!(v._pyobject = PyInt_FromLong(0)))
      py_error("Error creating 0 number");

    // v._pyobject = Integer_Zero;
    // Py_INCREF(v._pyobject);
  }

  Number_T::Number_T(const int& x) { 
    verbose("Number_T::Number_T(const int& x)");
    t = PYOBJECT;
    //if (!(v._pyobject = PyObject_CallFunction(pyfunc_Integer, "i", x)))
    if (!(v._pyobject = PyInt_FromLong(x)))
      py_error("Error creating int");
    //t = LONG;
    //v._long = x;
  }
  Number_T::Number_T(const long int& x) { 
    verbose("Number_T::Number_T(const long int& x)");
    //t = LONG;
    //v._long = x;

    t = PYOBJECT;
    //if (!(v._pyobject = PyObject_CallFunction(pyfunc_Integer, "l", x)))
    if (!(v._pyobject = PyInt_FromLong(x)))
      py_error("Error creating long int");

    //t = PYOBJECT;
    //v._pyobject = PyInt_FromLong(x);
  }

  Number_T::Number_T(const unsigned int& x) { 
    verbose("Number_T::Number_T(const unsigned int& x)");
    t = PYOBJECT;
    if (!(v._pyobject = PyObject_CallFunction(pyfunc_Integer, "I", x)))
      py_error("Error creating unsigned long int");
  }

  Number_T::Number_T(const unsigned long& x) { 
    verbose("Number_T::Number_T(const unsigned long& x)");
    t = PYOBJECT;
    if (!(v._pyobject = PyObject_CallFunction(pyfunc_Integer, "k", x)))
      py_error("Error creating unsigned long int");
  }

  Number_T::Number_T(const double& x) { 
    verbose("Number_T::Number_T(const double& x)");
    //t = DOUBLE;
    //v._double = x; 

    t = PYOBJECT;
    //if (!(v._pyobject = PyObject_CallFunction(pyfunc_Float, "d", x)))
    if (!(v._pyobject =  PyFloat_FromDouble(x)))
      py_error("Error creating double");

    //t = PYOBJECT;
    //v._pyobject = PyFloat_FromDouble(x);
  }

  Number_T::Number_T(const Number_T& x) { 
    verbose("Number_T::Number_T(const Number_T& x)");
    t = x.t;
    switch(t) {
    case DOUBLE:
      v._double = x.v._double;
      return;
    case LONG:
      v._long = x.v._long;
      return;
    case PYOBJECT:
      v._pyobject = x.v._pyobject;
      Py_INCREF(v._pyobject);
      return;
    default:
      stub("Number_T(const Number_T& x) type not handled");
    }
  }

  Number_T::Number_T(const char* s) { 
    stub("Number_T(const char* s)");
    t = DOUBLE;
    sscanf(s, "%f", &v._double); 
  }

  Number_T::Number_T(PyObject* o) {
    verbose("Number_T::Number_T(PyObject* o)");
    t = PYOBJECT;
    if (!o) {
      // TODO: something bad happened -- an exception; figure out how to deal with this.
      std::cerr << "ERROR IN Sage-GINAC; object SET TO ZERO";
      v._pyobject = PyInt_FromLong(0);
      return;
    }
    // STEAL a reference
    v._pyobject = o;
  }
  
  Number_T::~Number_T() {
    switch(t) {
    case PYOBJECT:
      Py_DECREF(v._pyobject);
      return;
    }
  }
    
  Number_T Number_T::operator+(Number_T x) const { 
    verbose("operator+");
    // We will have to write a coercion model :-(
    // Or just a nested switch of switches that covers all
    // combinations of long,double,mpfr,mpz,mpq,mpfc,mpqc.  Yikes.
    if (t != x.t) {
      Number_T a, b;
      coerce(a, b, *this, x);
      return a + b;
    }
    switch(t) {
    case DOUBLE:
      return v._double + (double)x;
    case LONG:
      return v._long + (long int)x;
    case PYOBJECT:
      return PyNumber_Add(v._pyobject, x.v._pyobject);
    default:
      stub("operator+() type not handled");
    }
  }

  Number_T Number_T::operator*(Number_T x) const { 
    verbose("operator*");
    if (t != x.t) {
      Number_T a, b;
      coerce(a, b, *this, x);
      return a * b;
    }
    switch(t) {
    case DOUBLE:
      return v._double * (double)x;
    case LONG:
      return v._long * (long int)x;
    case PYOBJECT:
      return PyNumber_Multiply(v._pyobject, x.v._pyobject);
    default:
      stub("operator*() type not handled");
    }
  }

  Number_T Number_T::operator/(Number_T x) const { 
    verbose("operator/");
    if (t != x.t) {
      Number_T a, b;
      coerce(a, b, *this, x);
      return a / b;
    }
    switch(t) {
    case DOUBLE:
      return v._double / (double)x;
    case LONG:
      return v._long / (long int)x;
    case PYOBJECT:
      return PyNumber_Divide(v._pyobject, x.v._pyobject);
    default:
      stub("operator/() type not handled");
    }
  }

  Number_T Number_T::operator-(Number_T x) const { 
    verbose("operator-");
    if (t != x.t) {
      Number_T a, b;
      coerce(a, b, *this, x);
      return a - b;
    }
    switch(t) {
    case DOUBLE:
      return v._double - (double)x;
    case LONG:
      return v._long - (long int)x;
    case PYOBJECT:
      return PyNumber_Subtract(v._pyobject, x.v._pyobject);
    default:
      stub("operator-() type not handled");
    }
  }

  Number_T& Number_T::operator=(const Number_T& x) { 
    verbose("operator=");
    t = x.t;
    switch(x.t) {
    case DOUBLE:
      v._double = x.v._double; 
      break;

    case LONG:
      v._long = x.v._long; 
      break;

    case PYOBJECT:
      if (t == PYOBJECT) {
	Py_DECREF(v._pyobject);
      }
      Py_INCREF(x.v._pyobject);
      v._pyobject = x.v._pyobject; 
      break;
      
    default:
      stub("operator= -- not able to do conversion! now total nonsense");
      break;
    };
    return *this; 
  }
  
  Number_T Number_T::operator()(const int& x) { 
    verbose("operator()(const int)");
    return Number_T(x); 
  }

  Number_T Number_T::operator-() { 
    verbose("operator-");
    switch(t) {
    case DOUBLE:
      return -v._double; 
    case LONG:
      return -v._long;
    case PYOBJECT:
      return PyNumber_Negative(v._pyobject);
    default:
      stub("operator-() type not handled");
    }
  }

  Number_T::operator double() const { 
    verbose("operator double");
    switch(t) {
    case DOUBLE:
      return v._double; 
    case LONG:
      return (double) v._long;
    case PYOBJECT:
      PyFloat_AsDouble(v._pyobject);
    default:
      stub("operator double() type not handled");
    }
  }

  Number_T::operator int() const { 
    verbose("operator int");
    switch(t) {
    case DOUBLE:
      return (int) v._double; 
    case LONG:
      todo("Need to check for overflow in   Number_T::operator int() const");
      return (int) v._long;
    case PYOBJECT:
      return PyInt_AsLong(v._pyobject);
    default:
      stub("operator int() type not handled");
    }
  }

  Number_T::operator long int() const { 
    verbose("operator long int");
    switch(t) {
    case DOUBLE:
      return (long int) v._double; 
    case LONG:
      return (long int) v._long;
    case PYOBJECT:
      long int a = PyInt_AsLong(v._pyobject);
      if (a == -1 && PyErr_Occurred()) {
	PyErr_Print();
	py_error("Overfloat converting to long int");
      }
      return a;
    default:
      stub("operator long int() type not handled");
    }
  }

  unsigned Number_T::hash() const { 
    //verbose("hash");
    switch(t) {
    case DOUBLE:
      return (unsigned int) v._double; 
    case LONG:
      return (unsigned int) v._long;
    case PYOBJECT:
      return PyObject_Hash(v._pyobject);
    default:
      stub("::hash() type not handled");
    }
    //    return static_cast<unsigned>(v._double); 
  }
  
  bool Number_T::operator==(const Number_T& right) const { 
    verbose("operator==");
    if (t != right.t) {
      Number_T a, b;
      coerce(a, b, *this, right);
      return a == b;
    }
    switch(t) {
    case DOUBLE:
      return v._double == right.v._double;
    case LONG:
      return v._long == right.v._long;
    case PYOBJECT:
      int result;
      if (PyObject_Cmp(v._pyobject, right.v._pyobject, &result)== -1) {
	py_error("==");
      }
      return (result == 0);
    default:
      stub("operator== type not handled");
    }
  }

  bool Number_T::operator!=(const Number_T& right) const { 
    verbose("operator!=");
    if (t != right.t) {
      Number_T a, b;
      coerce(a, b, *this, right);
      return a != b;
    }
    switch(t) {
    case DOUBLE:
      return v._double != right.v._double;
    case LONG:
      return v._long != right.v._long;
    case PYOBJECT:
      int result;
      if (PyObject_Cmp(v._pyobject, right.v._pyobject, &result) == -1) {
	py_error("!=");
      }
      return (result != 0);
    default:
      stub("operator!= type not handled");
    }
  }

  bool Number_T::operator<=(const Number_T& right) const { 
    verbose("operator<=");
    if (t != right.t) {
      Number_T a, b;
      coerce(a, b, *this, right);
      return a <= b;
    }
    switch(t) {
    case DOUBLE:
      return v._double <= right.v._double;
    case LONG:
      return v._long <= right.v._long;
    case PYOBJECT:
      int result;
      if (PyObject_Cmp(v._pyobject, right.v._pyobject, &result) == -1) {
	py_error("<=");
      }
      return (result <= 0);
    default:
      stub("operator<= type not handled");
    }

  }
  
  bool Number_T::operator>=(const Number_T& right) const { 
    verbose("operator>=");
    if (t != right.t) {
      Number_T a, b;
      coerce(a, b, *this, right);
      return a >= b;
    }
    switch(t) {
    case DOUBLE:
      return v._double >= right.v._double;
    case LONG:
      return v._long >= right.v._long;
    case PYOBJECT:
      int result;
      if (PyObject_Cmp(v._pyobject, right.v._pyobject, &result) == -1) {
	py_error(">=");
      }
      return (result >= 0);
    default:
      stub("operator!= type not handled");
    }
  }

  bool Number_T::operator<(const Number_T& right) const {
    verbose("operator<");
    if (t != right.t) {
      Number_T a, b;
      coerce(a, b, *this, right);
      return a < b;
    }

    switch(t) {
    case DOUBLE:
      return v._double < right.v._double;
    case LONG:
      return v._long < right.v._long;
    case PYOBJECT:
      int result;
      if (PyObject_Cmp(v._pyobject, right.v._pyobject, &result) == -1) {
	py_error("<");
      }
      return (result < 0);
    default:
      stub("operator< type not handled");
    }
  }

  bool Number_T::operator>(const Number_T& right) const { 
    verbose("operator>");
    if (t != right.t) {
      Number_T a, b;
      coerce(a, b, *this, right);
      return a > b;
    }
    switch(t) {
    case DOUBLE:
      return v._double > right.v._double;
    case LONG:
      return v._long > right.v._long;
    case PYOBJECT:
      int result;
      if (PyObject_Cmp(v._pyobject, right.v._pyobject, &result) == -1) {
	py_error(">");
      }
      return (result > 0);
    default:
      stub("operator> type not handled");
    }
  }
  
  Number_T Number_T::inverse() const { 
    verbose("inverse");
    switch(t) {
    case DOUBLE:
      return 1/v._double; 
    case LONG:
      return 1/v._long;
    case PYOBJECT:
      // TODO: handle errors
      return PyNumber_Invert(v._pyobject);
    default:
      stub("inverse() type not handled");
    }
  }
  
  int Number_T::csgn() const { 
    verbose("csgn");
    switch(t) {
    case DOUBLE:
      if (v._double<0) 
	return -1; 
      if (v._double==0) 
	return 0; 
      return 1;
    case LONG:
      if (v._long<0) 
	return -1; 
      if (v._long==0) 
	return 0; 
      return 1;
    case PYOBJECT:
      int result;
      if (PyObject_Cmp(v._pyobject, ZERO, &result) == -1) 
	py_error("csgn");
      return result;
    default:
      stub("csgn() type not handled");
    }
  }

  bool Number_T::is_zero() const { 
    verbose("is_zero");
    switch(t) {
    case DOUBLE:
      return v._double == 0; 
    case LONG:
      return v._long == 0; 
    case PYOBJECT:
      return PyObject_Not(v._pyobject);
    default:
      stub("is_zero() type not handled");
    }
  }

  bool Number_T::is_positive() const { 
    verbose("is_positive");
    switch(t) {
    case DOUBLE:
      return v._double > 0; 
    case LONG:
      return v._long > 0; 
    case PYOBJECT:
      return PyObject_Compare(v._pyobject, ZERO) > 0;
    default:
      stub("is_positive() type not handled");
    }
  }

  bool Number_T::is_negative() const { 
    verbose("is_negative");
    switch(t) {
    case DOUBLE:
      return v._double < 0; 
    case LONG:
      return v._long < 0; 
    case PYOBJECT:
      return PyObject_Compare(v._pyobject, ZERO) < 0;
    default:
      stub("is_negative() type not handled");
    }
  }

  bool Number_T::is_integer() const { 
    verbose("is_integer");
    switch(t) {
    case DOUBLE:
      return false;
    case LONG:
      return true;
    case PYOBJECT:
      Py_INCREF(v._pyobject);  // is this right?
      PyObject* o = PyObject_CallFunctionObjArgs(pyfunc_Integer, v._pyobject, NULL);
      bool ans = o;
      Py_DECREF(o);
      return ans;
    default:
      stub("is_integer() type not handled");
    }
  }

  bool Number_T::is_pos_integer() const { 
    verbose("is_pos_integer");
    switch(t) {
    case DOUBLE:
      return false;
    case LONG:
      return (v._long > 0);
    case PYOBJECT:
      return (is_integer() && is_positive());
    default:
      stub("is_pos_integer() type not handled");
    }
  }

  bool Number_T::is_nonneg_integer() const { 
    verbose("is_nonneg_integer");
    switch(t) {
    case DOUBLE:
      return false;
    case LONG:
      return (v._long >= 0);
    case PYOBJECT:
      return (is_integer() && (PyObject_Compare(v._pyobject, ZERO) >= 0));
    default:
      stub("is_nonneg_integer() type not handled");
    }
  }
  
  bool Number_T::is_even() const { 
    verbose("is_even");
    switch(t) {
    case DOUBLE:
      return false;
    case LONG:
      return (v._long %2 == 0);
    case PYOBJECT:
      PyObject* o = PyNumber_Remainder(v._pyobject, TWO);
      if (!o) {
	Py_DECREF(o);
	return false;
      }
      bool ans = (PyObject_Compare(o, ZERO) == 0);
      Py_DECREF(o);
      return ans;
    default:
      stub("is_even() type not handled");
    }
  }

  bool Number_T::is_odd() const { 
    switch(t) {
    case DOUBLE:
      return false;
    case LONG:
      return (v._long %2 == 1);
    case PYOBJECT:
      return !is_even();
    default:
      stub("is_odd() type not handled");
    }
  }
  
  bool Number_T::is_prime() const { 
    verbose("is_prime");
    switch(t) {
    case DOUBLE:
      return false;
    case LONG:
      stub("is_prime not implemented");
      return false;
    case PYOBJECT:
      stub("is_prime not implemented for pyobjects");
    default:
      stub("is_prime() type not handled");
    }
  }
   
  bool Number_T::is_rational() const { 
    verbose("is_rational");
    switch(t) {
    case DOUBLE:
      return false;
    case LONG:
      return true;
    case PYOBJECT:
      //TODO: stub("pyobject is_rational() -- faking true");
      return true;
    default:
      stub("is_rational() type not handled");
    }
  }

  bool Number_T::is_real() const { 
    verbose("is_real");
    switch(t) {
    case DOUBLE:
    case LONG:
      return true;
    default:
      stub("is_real() type not handled");
    }
  }

  Number_T Number_T::numer() const { 
    verbose2("numer -- in:", *this);
    Number_T ans;

    switch(t) {

    case DOUBLE:
    case LONG:
      ans = *this;
      break;

    case PYOBJECT:
      if (PyInt_Check(v._pyobject)) {
	ans = *this;
	break;
      } else {
	PyObject* o = PyObject_CallMethodObjArgs(v._pyobject, s_numerator, NULL);
	if (!o) {
	  verbose("call to numerator failed.");
	  ans = *this;
	} else {
	  ans = o;
	}
      }
      break;
    default:
      stub("numer() type not handled");
      ans = *this;
    }
    verbose2("numer -- out:", ans);
    return ans;
  }

  Number_T Number_T::denom() const { 
    verbose2("denom -- in:", *this);
    Number_T ans;

    switch(t) {
    case DOUBLE:
    case LONG:
      ans = 1;
      break;

    case PYOBJECT:
      if (PyInt_Check(v._pyobject)) {
	ans = ONE;
      } else {
	Py_INCREF(v._pyobject);  // is this right?
	Py_INCREF(s_denominator);
	PyObject* o = PyObject_CallMethodObjArgs(v._pyobject, s_denominator, NULL);
	if (!o) {
	  verbose("call to denom failed.");
	  ans = ONE;
	} else {
	  ans = o;
	}
      }
      break;

    default:
      stub("denom() type not handled");
      ans = ONE;
    }
    verbose2("denom -- out:", ans);
    return ans;
  }
  
  Number_T Number_T::lcm(Number_T b) const { 
    verbose3("lcm: in -- ",*this,b);
    Number_T ans;
    if (t != b.t) {
      Number_T a, c;
      coerce(a, c, *this, b);
      ans = a.lcm(c);
      verbose2("lcm: out (coercion) -- ", ans);
      return ans;
    }

    switch(t) {
    case DOUBLE:
      if (v._double == 0 && b.v._double==0)
	ans = 0.0;
      else
	ans = 1.0;
      break;

    case LONG:
      ans = (v._long * b.v._long) / gcd_long(v._long, b.v._long);
      break;

    case PYOBJECT:
      PyObject* o;
      Py_INCREF(v._pyobject);
      Py_INCREF(b.v._pyobject);
      if (! (o = PyObject_CallFunctionObjArgs(pyfunc_lcm, v._pyobject, b.v._pyobject, NULL)) ) {
	py_error("lcm()");
      }
      ans = o;
      break;
      //PyObject* o = PyObject_CallMethodObjArgs(v._pyobject, s_lcm, b.v._pyobject, NULL);
      //if (!o) {
      //py_error("lcm()");
      //}
      //return o;
    default:
      stub("lcm() type not handled");
    }
    verbose2("lcm: out -- ",ans);
  }

  Number_T Number_T::gcd(Number_T b) const { 
    verbose3("gcd: in -- ",*this,b);
    Number_T ans;
    if (t != b.t) {
      Number_T a, c;
      coerce(a, c, *this, b);
      ans = a.gcd(c);
      verbose2("gcd: out (coercion) -- ", ans);
      return ans;
    }
    switch(t) {
    case DOUBLE:
      verbose("it is a double gcd");
      if (v._double == 0 && b.v._double==0)  
	ans = 0.0;
      else
	ans = 1.0;
      break;

    case LONG:
      verbose("it is a long gcd");
      ans = gcd_long(v._long, b.v._long);
      break;

    case PYOBJECT:
      verbose("it is a pyobject gcd");
      verbose3("gcd in from pyobjects", Number_T(v._pyobject), Number_T(b.v._pyobject));
      PyObject* o;
      Py_INCREF(v._pyobject);
      Py_INCREF(b.v._pyobject);
      if (! (o = PyObject_CallFunctionObjArgs(pyfunc_gcd, v._pyobject, b.v._pyobject, NULL)) ) {
	py_error("gcd()");
      }
      ans = o;
      //PyObject* o = PyObject_CallMethodObjArgs(v._pyobject, s_gcd, b.v._pyobject, NULL);
      //if (!o) {
      //py_error("gcd()");
      //}
      break;

    default:
      stub("gcd() type not handled");
    }
    verbose2("gcd: out -- ",ans);
  }

  

  ///////////////////////////////////////////////////////////////////////////////
  // class numeric
  ///////////////////////////////////////////////////////////////////////////////

  GINAC_IMPLEMENT_REGISTERED_CLASS_OPT(numeric, basic,
				       print_func<print_context>(&numeric::do_print).
				       print_func<print_latex>(&numeric::do_print_latex).
				       print_func<print_csrc>(&numeric::do_print_csrc).
				       print_func<print_tree>(&numeric::do_print_tree).
				       print_func<print_python_repr>(&numeric::do_print_python_repr))


  //////////
  // default constructor
  //////////

  /** default constructor. Numerically it initializes to an integer zero. */
  numeric::numeric() : basic(&numeric::tinfo_static), 
    		       value(0)
  {
    setflag(status_flags::evaluated | status_flags::expanded);
  }

  //////////
  // other constructors
  //////////

  // public

  numeric::numeric(PyObject* o) : basic(&numeric::tinfo_static), 
				  value(o)
  {
    setflag(status_flags::evaluated | status_flags::expanded);
  }

  numeric::numeric(int i) : basic(&numeric::tinfo_static), 
			    value(i)
  {
    setflag(status_flags::evaluated | status_flags::expanded);
  }


  numeric::numeric(unsigned int i) : basic(&numeric::tinfo_static),
				     value(i)
  {
    setflag(status_flags::evaluated | status_flags::expanded);
  }


  numeric::numeric(long i) : basic(&numeric::tinfo_static),
			     value(i)
  {
    setflag(status_flags::evaluated | status_flags::expanded);
  }


  numeric::numeric(unsigned long i) : basic(&numeric::tinfo_static),
				      value(i)
  {
    setflag(status_flags::evaluated | status_flags::expanded);
  }


  /** Constructor for rational numerics a/b.
   *
   *  @exception overflow_error (division by zero) */
  numeric::numeric(long numer, long denom) : basic(&numeric::tinfo_static)
  {
    if (!denom)
      ginac_error("numeric::div(): division by zero");
    // throw std::overflow_error("division by zero");
    value = Number_T(numer) / Number_T(denom);
    setflag(status_flags::evaluated | status_flags::expanded);
  }

  numeric::numeric(double d) : basic(&numeric::tinfo_static), 
			       value(d)
  {
    setflag(status_flags::evaluated | status_flags::expanded);
  }


  numeric::numeric(const Number_T& x) : basic(&numeric::tinfo_static),
					value(x)
  {
    setflag(status_flags::evaluated | status_flags::expanded);
  }

  /** constructor from C-style string.  It also accepts complex numbers in GiNaC
   *  notation like "2+5*I". */
  numeric::numeric(const char *s) : value(s),
				    basic(&numeric::tinfo_static)
  {
    setflag(status_flags::evaluated | status_flags::expanded);
  }


  //////////
  // archiving
  //////////

  numeric::numeric(const archive_node &n, lst &sym_lst) : inherited(n, sym_lst)
  {
    stub("archiving");
    setflag(status_flags::evaluated | status_flags::expanded);
  }

  void numeric::archive(archive_node &n) const
  {
    stub("archive");
    inherited::archive(n);
  }

  DEFAULT_UNARCHIVE(numeric)

  //////////
  // functions overriding virtual functions from base classes
  //////////

  /** 
   *
   *  @see numeric::print() */
  static void print_real_number(const print_context & c, const Number_T & x)
  {
    c.s << x;
  }

  /** Helper function to print integer number in C++ source format.
   *
   *  @see numeric::print() */
  static void print_integer_csrc(const print_context & c, const Number_T & x)
  {
    stub("print_integer_csrc");
  }
  /** Helper function to print real number in C++ source format.
   *
   *  @see numeric::print() */
  static void print_real_csrc(const print_context & c, const Number_T & x)
  {
    stub("print_real_csrc");
  }

  template<typename T1, typename T2> 
  static inline bool coerce(T1& dst, const T2& arg);

  /** 
   * @brief Check if CLN integer can be converted into int
   *
   * @sa http://www.ginac.de/pipermail/cln-list/2006-October/000248.html
   */
  template<>
  inline bool coerce<int, Number_T>(int& dst, const Number_T& arg)
  {
    dst = (int) arg;
  }

  template<>
  inline bool coerce<unsigned int, Number_T>(unsigned int& dst, const Number_T& arg)
  {
    dst = (long int) arg;   // TODO: worry about long int to unsigned int!!
  }

  void numeric::print_numeric(const print_context & c, const char *par_open, const char *par_close, const char *imag_sym, const char *mul_sym, unsigned level) const
  {
    c.s << value;
  }

  void numeric::do_print(const print_context & c, unsigned level) const
  {
    print_numeric(c, "(", ")", "I", "*", level);
  }

  void numeric::do_print_latex(const print_latex & c, unsigned level) const
  {
    print_numeric(c, "{(", ")}", "i", " ", level);
  }

  void numeric::do_print_csrc(const print_csrc & c, unsigned level) const
  {
    stub("print_csrc");
  }


  void numeric::do_print_tree(const print_tree & c, unsigned level) const
  {
    c.s << std::string(level, ' ') << value
	<< " (" << class_name() << ")" << " @" << this
	<< std::hex << ", hash=0x" << hashvalue << ", flags=0x" << flags << std::dec
	<< std::endl;
  }

  void numeric::do_print_python_repr(const print_python_repr & c, unsigned level) const
  {
    c.s << class_name() << "('";
    print_numeric(c, "(", ")", "I", "*", level);
    c.s << "')";
  }

  bool numeric::info(unsigned inf) const
  {
    switch (inf) {
    case info_flags::numeric:
    case info_flags::polynomial:
    case info_flags::rational_function:
    case info_flags::expanded:
      return true;
    case info_flags::real:
      return is_real();
    case info_flags::rational:
    case info_flags::rational_polynomial:
      return is_rational();
    case info_flags::crational:
    case info_flags::crational_polynomial:
      return is_crational();
    case info_flags::integer:
    case info_flags::integer_polynomial:
      return is_integer();
    case info_flags::cinteger:
    case info_flags::cinteger_polynomial:
      return is_cinteger();
    case info_flags::positive:
      return is_positive();
    case info_flags::negative:
      return is_negative();
    case info_flags::nonnegative:
      return !is_negative();
    case info_flags::posint:
      return is_pos_integer();
    case info_flags::negint:
      return is_integer() && is_negative();
    case info_flags::nonnegint:
      return is_nonneg_integer();
    case info_flags::even:
      return is_even();
    case info_flags::odd:
      return is_odd();
    case info_flags::prime:
      return is_prime();
    case info_flags::algebraic:
      return !is_real();
    }
    return false;
  }

  bool numeric::is_polynomial(const ex & var) const
  {
    return true;
  }

  int numeric::degree(const ex & s) const
  {
    // In sage deg (0) != 0 !!!
    return 0;
  }

  int numeric::ldegree(const ex & s) const
  {
    return 0;
  }

  ex numeric::coeff(const ex & s, int n) const
  {
    return n==0 ? *this : _ex0;
  }

  /** Disassemble real part and imaginary part to scan for the occurrence of a
   *  single number.  Also handles the imaginary unit.  It ignores the sign on
   *  both this and the argument, which may lead to what might appear as funny
   *  results:  (2+I).has(-2) -> true.  But this is consistent, since we also
   *  would like to have (-2+I).has(2) -> true and we want to think about the
   *  sign as a multiplicative factor. */
  bool numeric::has(const ex &other, unsigned options) const
  {
    if (!is_exactly_a<numeric>(other))
      return false;
    const numeric &o = ex_to<numeric>(other);
    if (this->is_equal(o) || this->is_equal(-o))
      return true;
    if (o.imag().is_zero()) {   // e.g. scan for 3 in -3*I
      if (!this->real().is_equal(*_num0_p))
	if (this->real().is_equal(o) || this->real().is_equal(-o))
	  return true;
      if (!this->imag().is_equal(*_num0_p))
	if (this->imag().is_equal(o) || this->imag().is_equal(-o))
	  return true;
      return false;
    }
    else {
      if (o.is_equal(I))  // e.g scan for I in 42*I
	return !this->is_real();
      if (o.real().is_zero())  // e.g. scan for 2*I in 2*I+1
	if (!this->imag().is_equal(*_num0_p))
	  if (this->imag().is_equal(o*I) || this->imag().is_equal(-o*I))
	    return true;
    }
    return false;
  }


  /** Evaluation of numbers doesn't do anything at all. */
  ex numeric::eval(int level) const
  {
    // Warning: if this is ever gonna do something, the ex constructors from all kinds
    // of numbers should be checking for status_flags::evaluated.
    return this->hold();
  }


  /** Cast numeric into a floating-point object.  For example exact numeric(1) is
   *  returned as a 1.0000000000000000000000 and so on according to how Digits is
   *  currently set.  In case the object already was a floating point number the
   *  precision is trimmed to match the currently set default.
   *
   *  @param level  ignored, only needed for overriding basic::evalf.
   *  @return  an ex-handle to a numeric. */
  ex numeric::evalf(int level) const
  {
    stub("evalf");
  }

  ex numeric::conjugate() const
  {
    if (is_real()) 
      return *this;

    stub("conjugate of nonreal");
  }

  ex numeric::real_part() const
  {
    if (is_real()) 
      return *this;
    stub("real_part of complex");
  }

  ex numeric::imag_part() const
  {
    if (is_real())
      return 0;
    stub("imag_part of complex");
  }

  // protected

  int numeric::compare_same_type(const basic &other) const
  {
    GINAC_ASSERT(is_exactly_a<numeric>(other));
    const numeric &o = static_cast<const numeric &>(other);
	
    return this->compare(o);
  }


  bool numeric::is_equal_same_type(const basic &other) const
  {
    GINAC_ASSERT(is_exactly_a<numeric>(other));
    const numeric &o = static_cast<const numeric &>(other);
	
    return this->is_equal(o);
  }


  unsigned numeric::calchash() const
  {
    return value.hash();
  }


  //////////
  // new virtual functions which can be overridden by derived classes
  //////////

  // none

  //////////
  // non-virtual functions in this class
  //////////

  // public

  /** Numerical addition method.  Adds argument to *this and returns result as
   *  a numeric object. */
  const numeric numeric::add(const numeric &other) const
  {
    return value + other.value;
  }


  /** Numerical subtraction method.  Subtracts argument from *this and returns
   *  result as a numeric object. */
  const numeric numeric::sub(const numeric &other) const
  {
    return value - other.value;
  }


  /** Numerical multiplication method.  Multiplies *this and argument and returns
   *  result as a numeric object. */
  const numeric numeric::mul(const numeric &other) const
  {
    return value * other.value;
  }


  /** Numerical division method.  Divides *this by argument and returns result as
   *  a numeric object.
   *
   *  @exception overflow_error (division by zero) */
  const numeric numeric::div(const numeric &other) const
  {
    if (other.is_zero()) 
      ginac_error("numeric::div(): division by zero");
    //throw std::overflow_error("numeric::div(): division by zero");
    return value / other.value;
  }


  /** Numerical exponentiation.  Raises *this to the power given as argument and
   *  returns result as a numeric object. */
  const numeric numeric::power(const numeric &other) const
  {
    return pow(value, other.value);
  }

  /** Numerical addition method.  Adds argument to *this and returns result as
   *  a numeric object on the heap.  Use internally only for direct wrapping into
   *  an ex object, where the result would end up on the heap anyways. */
  const numeric &numeric::add_dyn(const numeric &other) const
  {
    // Efficiency shortcut: trap the neutral element by pointer.  This hack
    // is supposed to keep the number of distinct numeric objects low.
    if (this==_num0_p)
      return other;
    else if (&other==_num0_p)
      return *this;
	
    return static_cast<const numeric &>((new numeric(value + other.value))->
					setflag(status_flags::dynallocated));
  }


  /** Numerical subtraction method.  Subtracts argument from *this and returns
   *  result as a numeric object on the heap.  Use internally only for direct
   *  wrapping into an ex object, where the result would end up on the heap
   *  anyways. */
  const numeric &numeric::sub_dyn(const numeric &other) const
  {
    // Efficiency shortcut: trap the neutral exponent (first by pointer).  This
    // hack is supposed to keep the number of distinct numeric objects low.
    if (&other==_num0_p || (other.value.is_zero()))
      return *this;
	
    return static_cast<const numeric &>((new numeric(value - other.value))->
					setflag(status_flags::dynallocated));
  }


  /** Numerical multiplication method.  Multiplies *this and argument and returns
   *  result as a numeric object on the heap.  Use internally only for direct
   *  wrapping into an ex object, where the result would end up on the heap
   *  anyways. */
  const numeric &numeric::mul_dyn(const numeric &other) const
  {
    // Efficiency shortcut: trap the neutral element by pointer.  This hack
    // is supposed to keep the number of distinct numeric objects low.
    if (this==_num1_p)
      return other;
    else if (&other==_num1_p)
      return *this;
	
    return static_cast<const numeric &>((new numeric(value * other.value))->
					setflag(status_flags::dynallocated));
  }


  /** Numerical division method.  Divides *this by argument and returns result as
   *  a numeric object on the heap.  Use internally only for direct wrapping
   *  into an ex object, where the result would end up on the heap
   *  anyways.
   *
   *  @exception overflow_error (division by zero) */
  const numeric &numeric::div_dyn(const numeric &other) const
  {
    // Efficiency shortcut: trap the neutral element by pointer.  This hack
    // is supposed to keep the number of distinct numeric objects low.
    if (&other==_num1_p)
      return *this;
    if (other.value.is_zero())
      ginac_error("division by zero");
      //throw std::overflow_error("division by zero");
    return static_cast<const numeric &>((new numeric(value / other.value))->
					setflag(status_flags::dynallocated));
  }


  /** Numerical exponentiation.  Raises *this to the power given as argument and
   *  returns result as a numeric object on the heap.  Use internally only for
   *  direct wrapping into an ex object, where the result would end up on the
   *  heap anyways. */
  const numeric &numeric::power_dyn(const numeric &other) const
  {
    // Efficiency shortcut: trap the neutral exponent (first try by pointer, then
    // try harder, since calls to cln::expt() below may return amazing results for
    // floating point exponent 1.0).
    if (&other==_num1_p || (other.value == _num1_p->value))
      return *this;
	
    return static_cast<const numeric &>((new numeric(pow(value, other.value)))->
					setflag(status_flags::dynallocated));
  }


  const numeric &numeric::operator=(int i)
  {
    return operator=(numeric(i));
  }


  const numeric &numeric::operator=(unsigned int i)
  {
    return operator=(numeric(i));
  }


  const numeric &numeric::operator=(long i)
  {
    return operator=(numeric(i));
  }


  const numeric &numeric::operator=(unsigned long i)
  {
    return operator=(numeric(i));
  }


  const numeric &numeric::operator=(double d)
  {
    return operator=(numeric(d));
  }


  const numeric &numeric::operator=(const char * s)
  {
    return operator=(numeric(s));
  }


  /** Inverse of a number. */
  const numeric numeric::inverse() const
  {
    if (value.is_zero())
      ginac_error("numeric::inverse(): division by zero");
      // throw std::overflow_error("numeric::inverse(): division by zero");
    return numeric(value.inverse());
  }

  /** Return the step function of a numeric. The imaginary part of it is
   *  ignored because the step function is generally considered real but
   *  a numeric may develop a small imaginary part due to rounding errors.
   */
  numeric numeric::step() const
  {
    stub("step function");
  }

  /** Return the complex half-plane (left or right) in which the number lies.
   *  csgn(x)==0 for x==0, csgn(x)==1 for Re(x)>0 or Re(x)=0 and Im(x)>0,
   *  csgn(x)==-1 for Re(x)<0 or Re(x)=0 and Im(x)<0.
   *
   *  @see numeric::compare(const numeric &other) */
  int numeric::csgn() const
  {
    return value.csgn();
  }


  /** This method establishes a canonical order on all numbers.  For complex
   *  numbers this is not possible in a mathematically consistent way but we need
   *  to establish some order and it ought to be fast.  So we simply define it
   *  to be compatible with our method csgn.
   *
   *  @return csgn(*this-other)
   *  @see numeric::csgn() */
  int numeric::compare(const numeric &other) const
  {
    if (value < other.value)
      return -1;
    else if (value > other.value)
      return 1;
    return 0;
  }


  bool numeric::is_equal(const numeric &other) const
  {
    return value == other.value;
  }


  /** True if object is zero. */
  bool numeric::is_zero() const
  {
    return value.is_zero();
  }


  /** True if object is not complex and greater than zero. */
  bool numeric::is_positive() const
  {
    return value.is_positive();
  }


  /** True if object is not complex and less than zero. */
  bool numeric::is_negative() const
  {
    return value.is_negative();
  }


  /** True if object is a non-complex integer. */
  bool numeric::is_integer() const
  {
    return value.is_integer();
  }


  /** True if object is an exact integer greater than zero. */
  bool numeric::is_pos_integer() const
  {
    return value.is_pos_integer();
  }


  /** True if object is an exact integer greater or equal zero. */
  bool numeric::is_nonneg_integer() const
  {
    return value.is_nonneg_integer();
  }


  /** True if object is an exact even integer. */
  bool numeric::is_even() const
  {
    return value.is_even();
  }


  /** True if object is an exact odd integer. */
  bool numeric::is_odd() const
  {
    return value.is_odd();
  }


  /** Probabilistic primality test.
   *
   *  @return  true if object is exact integer and prime. */
  bool numeric::is_prime() const
  {
    return value.is_prime();
  }


  /** True if object is an exact rational number, may even be complex
   *  (denominator may be unity). */
  bool numeric::is_rational() const
  {
    return value.is_rational();
  }


  /** True if object is a real integer, rational or float (but not complex). */
  bool numeric::is_real() const
  {
    return value.is_real();
  }


  bool numeric::operator==(const numeric &other) const
  {
    return value == other.value;
  }

  bool numeric::operator!=(const numeric &other) const
  {
    return value != other.value;
  }


  /** True if object is element of the domain of integers extended by I, i.e. is
   *  of the form a+b*I, where a and b are integers. */
  bool numeric::is_cinteger() const
  {
   // TODO: I just call is_integer here.  Need something better
   // that takes into account what cinteger actually means??
    return value.is_integer();
  }


  /** True if object is an exact rational number, may even be complex
   *  (denominator may be unity). */
  bool numeric::is_crational() const
 {
   // TODO: I just call is_rational here.  Need something better
   // that takes into account what crational actually means??
    return value.is_rational();
  }


  /** Numerical comparison: less.
   *
   *  @exception invalid_argument (complex inequality) */ 
  bool numeric::operator<(const numeric &other) const
  {
    return value < other.value;
  }


  /** Numerical comparison: less or equal.
   *
   *  @exception invalid_argument (complex inequality) */ 
  bool numeric::operator<=(const numeric &other) const
  {
    return value <= other.value;
  }


  /** Numerical comparison: greater.
   *
   *  @exception invalid_argument (complex inequality) */ 
  bool numeric::operator>(const numeric &other) const
  {
    return value > other.value;
  }


  /** Numerical comparison: greater or equal.
   *
   *  @exception invalid_argument (complex inequality) */  
  bool numeric::operator>=(const numeric &other) const
  {
    return value >= other.value;
  }


  /** Converts numeric types to machine's int.  You should check with
   *  is_integer() if the number is really an integer before calling this method.
   *  You may also consider checking the range first. */
  int numeric::to_int() const
  {
    GINAC_ASSERT(this->is_integer());
    return (int) value;
  }


  /** Converts numeric types to machine's long.  You should check with
   *  is_integer() if the number is really an integer before calling this method.
   *  You may also consider checking the range first. */
  long numeric::to_long() const
  {
    GINAC_ASSERT(this->is_integer());
    return (long)(value);
  }


  /** Converts numeric types to machine's double. You should check with is_real()
   *  if the number is really not complex before calling this method. */
  double numeric::to_double() const
  {
    GINAC_ASSERT(this->is_real());
    return (double)(value); //more to be done
  }

  Number_T numeric::to_cl_N() const 
  {
    return value;
  }

  /** Real part of a number. */
  const numeric numeric::real() const
  {
    stub("numeric::real");
    return value; //TODO
  }


  /** Imaginary part of a number. */
  const numeric numeric::imag() const
  {
    stub("numeric::imag");
    return 0; //TODO
  }


  /** Numerator.  Computes the numerator of rational numbers, rationalized
   *  numerator of complex if real and imaginary part are both rational numbers
   *  (i.e numer(4/3+5/6*I) == 8+5*I), the number carrying the sign in all other
   *  cases. */
  const numeric numeric::numer() const
  {
    return value.numer();
  }


  /** Denominator.  Computes the denominator of rational numbers, common integer
   *  denominator of complex if real and imaginary part are both rational numbers
   *  (i.e denom(4/3+5/6*I) == 6), one in all other cases. */
  const numeric numeric::denom() const
  {
    return value.denom();
  }


  /** Size in binary notation.  For integers, this is the smallest n >= 0 such
   *  that -2^n <= x < 2^n. If x > 0, this is the unique n > 0 such that
   *  2^(n-1) <= x < 2^n.
   *
   *  @return  number of bits (excluding sign) needed to represent that number
   *  in two's complement if it is an integer, 0 otherwise. */    
  int numeric::int_length() const
  {
    stub("int_length");
    return 0;
  }

  //////////
  // global constants
  //////////

  /** Imaginary unit.  This is not a constant but a numeric since we are
   *  natively handing complex numbers anyways, so in each expression containing
   *  an I it is automatically eval'ed away anyhow. */

  const numeric I = 1; //TODO


  /** Exponential function.
   *
   *  @return  arbitrary precision numerical exp(x). */
  const numeric exp(const numeric &x)
  {
    return exp(x.value);
  }


  /** Natural logarithm.
   *
   *  @param x complex number
   *  @return  arbitrary precision numerical log(x).
   *  @exception pole_error("log(): logarithmic pole",0) */
  const numeric log(const numeric &x)
  {
    if (x.is_zero())
      ginac_error("log(): logarithmic pole");
     //throw pole_error("log(): logarithmic pole",0);
    return log(x.value);
  }


  /** Numeric sine (trigonometric function).
   *
   *  @return  arbitrary precision numerical sin(x). */
  const numeric sin(const numeric &x)
  {
    return sin(x.value); 
  }


  /** Numeric cosine (trigonometric function).
   *
   *  @return  arbitrary precision numerical cos(x). */
  const numeric cos(const numeric &x)
  {
    return cos(x.value);
  }


  /** Numeric tangent (trigonometric function).
   *
   *  @return  arbitrary precision numerical tan(x). */
  const numeric tan(const numeric &x)
  {
    return tan(x.value);
  }
	

  /** Numeric inverse sine (trigonometric function).
   *
   *  @return  arbitrary precision numerical asin(x). */
  const numeric asin(const numeric &x)
  {
    return asin(x.value);
  }


  /** Numeric inverse cosine (trigonometric function).
   *
   *  @return  arbitrary precision numerical acos(x). */
  const numeric acos(const numeric &x)
  {
    return acos(x.value);
  }
	

  /** Numeric arcustangent.
   *
   *  @param x complex number
   *  @return atan(x)
   *  @exception pole_error("atan(): logarithmic pole",0) if x==I or x==-I. */
  const numeric atan(const numeric &x)
  {
    if (!x.is_real() &&
	x.real().is_zero() &&
	abs(x.imag()).is_equal(*_num1_p))
      ginac_error("atan(): logarithmic pole");
    //throw pole_error("atan(): logarithmic pole",0);
    return atan(x.value);
  }


  /** Numeric arcustangent of two arguments, analytically continued in a suitable way.
   *
   *  @param y complex number
   *  @param x complex number
   *  @return -I*log((x+I*y)/sqrt(x^2+y^2)), which is equal to atan(y/x) if y and
   *    x are both real.
   *  @exception pole_error("atan(): logarithmic pole",0) if y/x==+I or y/x==-I. */
  const numeric atan(const numeric &y, const numeric &x)
  {
    stub("atan");
  }


  /** Numeric hyperbolic sine (trigonometric function).
   *
   *  @return  arbitrary precision numerical sinh(x). */
  const numeric sinh(const numeric &x)
  {
    stub("sinh");
  }


  /** Numeric hyperbolic cosine (trigonometric function).
   *
   *  @return  arbitrary precision numerical cosh(x). */
  const numeric cosh(const numeric &x)
  {
    stub("cosh");
  }


  /** Numeric hyperbolic tangent (trigonometric function).
   *
   *  @return  arbitrary precision numerical tanh(x). */
  const numeric tanh(const numeric &x)
  {
    stub("tanh");
  }
	

  /** Numeric inverse hyperbolic sine (trigonometric function).
   *
   *  @return  arbitrary precision numerical asinh(x). */
  const numeric asinh(const numeric &x)
  {
    stub("asinh");
  }


  /** Numeric inverse hyperbolic cosine (trigonometric function).
   *
   *  @return  arbitrary precision numerical acosh(x). */
  const numeric acosh(const numeric &x)
  {
    stub("acosh");
  }


  /** Numeric inverse hyperbolic tangent (trigonometric function).
   *
   *  @return  arbitrary precision numerical atanh(x). */
  const numeric atanh(const numeric &x)
  {
    stub("atanh");
  }


  /*static cln::cl_N Li2_series(const ::cl_N &x,
    const ::float_format_t &prec)
    {
    // Note: argument must be in the unit circle
    // This is very inefficient unless we have fast floating point Bernoulli
    // numbers implemented!
    cln::cl_N c1 = -cln::log(1-x);
    cln::cl_N c2 = c1;
    // hard-wire the first two Bernoulli numbers
    cln::cl_N acc = c1 - cln::square(c1)/4;
    cln::cl_N aug;
    cln::cl_F pisq = cln::square(cln::cl_pi(prec));  // pi^2
    cln::cl_F piac = cln::cl_float(1, prec);  // accumulator: pi^(2*i)
    unsigned i = 1;
    c1 = cln::square(c1);
    do {
    c2 = c1 * c2;
    piac = piac * pisq;
    aug = c2 * (*(bernoulli(numeric(2*i)).clnptr())) / cln::factorial(2*i+1);
    // aug = c2 * cln::cl_I(i%2 ? 1 : -1) / cln::cl_I(2*i+1) * cln::cl_zeta(2*i, prec) / piac / (cln::cl_I(1)<<(2*i-1));
    acc = acc + aug;
    ++i;
    } while (acc != acc+aug);
    return acc;
    }*/

  /** Numeric evaluation of Dilogarithm within circle of convergence (unit
   *  circle) using a power series. */

  /** Numeric evaluation of Dilogarithm.  The domain is the entire complex plane,
   *  the branch cut lies along the positive real axis, starting at 1 and
   *  continuous with quadrant IV.
   *
   *  @return  arbitrary precision numerical Li2(x). */
  const numeric Li2(const numeric &x)
  {
    stub("Li2");
  }


  /** Numeric evaluation of Riemann's Zeta function.  Currently works only for
   *  integer arguments. */
  const numeric zeta(const numeric &x)
  {
    stub("zeta");
  }

  class lanczos_coeffs
  {
  public:
    lanczos_coeffs();
    bool sufficiently_accurate(int digits);
    int get_order() const { return current_vector->size(); }
    Number_T calc_lanczos_A(const Number_T &) const;
  private:
    // coeffs[0] is used in case Digits <= 20.
    // coeffs[1] is used in case Digits <= 50.
    // coeffs[2] is used in case Digits <= 100.
    // coeffs[3] is used in case Digits <= 200.
    static std::vector<Number_T> *coeffs;
    // Pointer to the vector that is currently in use.
    std::vector<Number_T> *current_vector;
  };

  std::vector<Number_T>* lanczos_coeffs::coeffs = 0;

  bool lanczos_coeffs::sufficiently_accurate(int digits) {
    stub("sufficiently_accurate");
  }

  Number_T lanczos_coeffs::calc_lanczos_A(const Number_T &x) const
  {
    stub("calc_lanczos_A");
  }

  // The values in this function have been calculated using the program
  // lanczos.cpp in the directory doc/examples. If you want to add more
  // digits, be sure to read the comments in that file.
  lanczos_coeffs::lanczos_coeffs()
  {
    stub("lanczos_coeffs");
  }


  /** The Gamma function.
   *  Use the Lanczos approximation. If the coefficients used here are not
   *  sufficiently many or sufficiently accurate, more can be calculated
   *  using the program doc/examples/lanczos.cpp. In that case, be sure to
   *  read the comments in that file. */
  const numeric lgamma(const numeric &x)
  {
    stub("lgamma");
  }

  const numeric tgamma(const numeric &x)
  {
    stub("tgamma");
  }


  /** The psi function (aka polygamma function).
   *  This is only a stub! */
  const numeric psi(const numeric &x)
  {
    stub("psi");
  }


  /** The psi functions (aka polygamma functions).
   *  This is only a stub! */
  const numeric psi(const numeric &n, const numeric &x)
  {
    stub("psi of two args");
  }


  /** Factorial combinatorial function.
   *
   *  @param n  integer argument >= 0
   *  @exception range_error (argument must be integer >= 0) */
  const numeric factorial(const numeric &n)
  {
    stub("factorial");
  }


  /** The double factorial combinatorial function.  (Scarcely used, but still
   *  useful in cases, like for exact results of tgamma(n+1/2) for instance.)
   *
   *  @param n  integer argument >= -1
   *  @return n!! == n * (n-2) * (n-4) * ... * ({1|2}) with 0!! == (-1)!! == 1
   *  @exception range_error (argument must be integer >= -1) */
  const numeric doublefactorial(const numeric &n)
  {
    stub("double factorial");
  }


  /** The Binomial coefficients.  It computes the binomial coefficients.  For
   *  integer n and k and positive n this is the number of ways of choosing k
   *  objects from n distinct objects.  If n is negative, the formula
   *  binomial(n,k) == (-1)^k*binomial(k-n-1,k) is used to compute the result. */
  const numeric binomial(const numeric &n, const numeric &k) {
    // If either fails below then an error will be raised.
    /*
    long int nn = n, kk = k;
    mpz_t rop;
    mpz_init(rop);
    mpz_bin_uiui(rop, nn, kk);
    // Now make something from a GMP integer?
    Number_T x = sage_integer(rop);
    mpz_clear(rop);
    return x;
    */
    PyObject* nn = to_pyobject(n.value);
    PyObject* kk = to_pyobject(k.value);
    Py_INCREF(nn);  // TODO --right?
    Py_INCREF(kk);  // TODO --right?
    PyObject* b = PyObject_CallFunctionObjArgs(pyfunc_binomial, nn, kk, NULL);
    if (!b) 
      py_error("binomial");
    Py_DECREF(nn);
    Py_DECREF(kk);
    return b;

    //std::cerr << "binomial(" << n << "," << k << ")\n";
    //stub("binomial");
    //return (long) binomial((long)n.value, (long)k.value);
  }


  /** Bernoulli number.  The nth Bernoulli number is the coefficient of x^n/n!
   *  in the expansion of the function x/(e^x-1).
   *
   *  @return the nth Bernoulli number (a rational number).
   *  @exception range_error (argument must be integer >= 0) */
  const numeric bernoulli(const numeric &nn)
  {
    stub("bernoulli");
  }


  /** Fibonacci number.  The nth Fibonacci number F(n) is defined by the
   *  recurrence formula F(n)==F(n-1)+F(n-2) with F(0)==0 and F(1)==1.
   *
   *  @param n an integer
   *  @return the nth Fibonacci number F(n) (an integer number)
   *  @exception range_error (argument must be an integer) */
  const numeric fibonacci(const numeric &n)
  {
    stub("fibonacci");
  }


  /** Absolute value. */
  const numeric abs(const numeric& x)
  {
    return abs(x.value);
  }


  /** Modulus (in positive representation).
   *  In general, mod(a,b) has the sign of b or is zero, and rem(a,b) has the
   *  sign of a or is zero. This is different from Maple's modp, where the sign
   *  of b is ignored. It is in agreement with Mathematica's Mod.
   *
   *  @return a mod b in the range [0,abs(b)-1] with sign of b if both are
   *  integer, 0 otherwise. */
  const numeric mod(const numeric &a, const numeric &b)
  {
    stub("mod");
    //return a.value.mod(b.value);
  }


  /** Modulus (in symmetric representation).
   *  Equivalent to Maple's mods.
   *
   *  @return a mod b in the range [-iquo(abs(b)-1,2), iquo(abs(b),2)]. */
  const numeric smod(const numeric &a, const numeric &b)
  {
    stub("smod");
    //return a.value.smod(b.value);
  }


  /** Numeric integer remainder.
   *  Equivalent to Maple's irem(a,b) as far as sign conventions are concerned.
   *  In general, mod(a,b) has the sign of b or is zero, and irem(a,b) has the
   *  sign of a or is zero.
   *
   *  @return remainder of a/b if both are integer, 0 otherwise.
   *  @exception overflow_error (division by zero) if b is zero. */
  const numeric irem(const numeric &a, const numeric &b)
  {
    stub("irem");
    //return a.value.irem(b.value);
  }


  /** Numeric integer remainder.
   *  Equivalent to Maple's irem(a,b,'q') it obeyes the relation
   *  irem(a,b,q) == a - q*b.  In general, mod(a,b) has the sign of b or is zero,
   *  and irem(a,b) has the sign of a or is zero.
   *
   *  @return remainder of a/b and quotient stored in q if both are integer,
   *  0 otherwise.
   *  @exception overflow_error (division by zero) if b is zero. */
  const numeric irem(const numeric &a, const numeric &b, numeric &q)
  {
    stub("irem");
  }


  /** Numeric integer quotient.
   *  Equivalent to Maple's iquo as far as sign conventions are concerned.
   *  
   *  @return truncated quotient of a/b if both are integer, 0 otherwise.
   *  @exception overflow_error (division by zero) if b is zero. */
  const numeric iquo(const numeric &a, const numeric &b)
  {
    stub("  const numeric iquo(const numeric &a, const numeric &b)");
  }


  /** Numeric integer quotient.
   *  Equivalent to Maple's iquo(a,b,'r') it obeyes the relation
   *  r == a - iquo(a,b,r)*b.
   *
   *  @return truncated quotient of a/b and remainder stored in r if both are
   *  integer, 0 otherwise.
   *  @exception overflow_error (division by zero) if b is zero. */
  const numeric iquo(const numeric &a, const numeric &b, numeric &r)
  {
    stub("  const numeric iquo(const numeric &a, const numeric &b, numeric &r)");
  }


  /** Greatest Common Divisor.
   *   
   *  @return  The GCD of two numbers if both are integer, a numerical 1
   *  if they are not. */
  const numeric gcd(const numeric &a, const numeric &b)
  {
    return a.value.gcd(b.value);
  }


  /** Least Common Multiple.
   *   
   *  @return  The LCM of two numbers if both are integer, the product of those
   *  two numbers if they are not. */
  const numeric lcm(const numeric &a, const numeric &b)
  {
    return a.value.lcm(b.value);
  }


  /** Numeric square root.
   *  If possible, sqrt(x) should respect squares of exact numbers, i.e. sqrt(4)
   *  should return integer 2.
   *
   *  @param x numeric argument
   *  @return square root of x. Branch cut along negative real axis, the negative
   *  real axis itself where imag(x)==0 and real(x)<0 belongs to the upper part
   *  where imag(x)>0. */
  const numeric sqrt(const numeric &x)
  {
    stub("const numeric sqrt(const numeric &x)");
  }


  /** Integer numeric square root. */
  const numeric isqrt(const numeric &x)
  {
    stub("const numeric isqrt(const numeric &x)");
  }


  /** Floating point evaluation of Archimedes' constant Pi. */
  ex PiEvalf()
  { 
    stub("ex PiEvalf");
  }


  /** Floating point evaluation of Euler's constant gamma. */
  ex EulerEvalf()
  { 
    stub("ex EulerEvalf");
  }


  /** Floating point evaluation of Catalan's constant. */
  ex CatalanEvalf()
  {
    stub("ex CatalanEvalf");
  }


  /** _numeric_digits default constructor, checking for singleton invariance. */
  _numeric_digits::_numeric_digits()
    : digits(17)
  {
  }


  /** Assign a native long to global Digits object. */
  _numeric_digits& _numeric_digits::operator=(long prec)
  {
    return *this;
  }


  /** Convert global Digits object to native type long. */
  _numeric_digits::operator long()
  {
    // BTW, this is approx. unsigned(cln::default_float_format*0.301)-1
    return (long)digits;
  }


  /** Append global Digits object to ostream. */
  void _numeric_digits::print(std::ostream &os) const
  {
    os << digits;
  }


  /** Add a new callback function. */
  void _numeric_digits::add_callback(digits_changed_callback callback)
  {
    callbacklist.push_back(callback);
  }


  std::ostream& operator<<(std::ostream &os, const _numeric_digits &e)
  {
    e.print(os);
    return os;
  }

  //////////
  // static member variables
  //////////

  // private

  bool _numeric_digits::too_late = false;


  /** Accuracy in decimal digits.  Only object of this type!  Can be set using
   *  assignment from C++ unsigned ints and evaluated like any built-in type. */
  _numeric_digits Digits;

} // namespace GiNaC
