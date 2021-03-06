#!/bin/bash
try() {
  expected="$1"
  input="$2"

  ./9cc "$input" > tmp.s
  gcc -o tmp tmp.s
  ./tmp
  actual="$?"

  if [ "$actual" = "$expected" ]; then
    echo "$input => $actual"
  else
    echo "$expected expected, but got $actual"
    exit 1
  fi
}

try 0 0
try 42 42
try 21 '5+20-4'
try 41 " 12 + 34 - 5 "
try 33 " 3 * 11 "
try 11 '110 /10 '
try 47 "5+6*7"
try 15 "5*(9-6)"
try 4 "(3+5)/2"
try 2 "11 % 3"
try 5 "-10+15"
try 4 " -5 + (-1) + 10"
try 1 "1 == 1"
try 0 " 300 == 200 "
try 1 "30!=20"
try 0 "6!= 6 "
try 1 '4 < 8'
try 0 ' 4 < 4'
try 1 " 99 <= 100"
try 1 "10<=10"
try 0 "  1919 <= 810"
try 1 " 893 >= 110"
try 1 "364 >=364 "
try 0 " 114>=514 "
try 1 " (5+6) == 11"
try 0 "5*2 == 11"
try 1 "(10 <= (10*10)) == ((3-2) >= 1)"

echo OK
