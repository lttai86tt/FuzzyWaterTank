#ifndef FUZZY_DEFUZZIFIER_H
#define FUZZY_DEFUZZIFIER_H
#pragma once

#include "class.h"
#include "classifier.h"

double calculateCentroid(MembershipFunction_t function, double membership);

double defuzzification(FuzzySet_t *set);

#endif
