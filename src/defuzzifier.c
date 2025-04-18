

#include "defuzzifier.h"

#include "class.h"
#include "classifier.h"
#include "membership_function.h"

/**
 * Calculate the centroid of a triangular membership function.
 *
 * @param function The triangular membership function to calculate the centroid
 * for.
 * @param membership The membership value of the function.
 * @return The centroid of the triangular membership function.
 */
double calculateTriangularCentroid(MembershipFunction_t function,
                                   double membership) {
    double a = function.a;
    double b = function.b;
    double c = function.c;

    if (membership == 0.0) {
        return 0.0;
    }

    if (a == b) {
        return b;
    }

    if (c == b) {
        return b;
    }

    double centroid = (a + b + c) / 3.0;
    return centroid;
}

/**
 * Calculate the centroid of a trapezoidal membership function.
 *
 * @param function The trapezoidal membership function to calculate the
 * centroid for.
 * @param membership The membership value of the function.
 * @return The centroid of the trapezoidal membership function.
 */
double calculateTrapezoidalCentroid(MembershipFunction_t function,
                                    double membership) {
    double a = function.a;
    double b = function.b;
    double c = function.c;
    double d = function.d;

    if (membership == 0.0) {
        return 0.0;
    }

    if (a == b && c == d) {
        return (b + c) / 2.0;
    }

    double centroid = (a + b + c + d) / 4.0;
    return centroid;
}

/**
 * Calculate the centroid of a rectangular membership function.
 *
 * @param function The rectangular membership function to calculate the
 * centroid for.
 * @param membership The membership value of the function.
 * @return The centroid of the rectangular membership function.
 */
double calculateRectangularCentroid(MembershipFunction_t function,
                                    double membership) {
    double a = function.a;
    double b = function.b;

    if (membership == 0.0) {
        return 0.0;
    }

    double centroid = (a + b) / 2.0;
    return centroid;
}

/**
 * Calculate the centroid of a membership function.
 *
 * @param function The membership function to calculate the centroid for.
 * @param membership The membership value of the function.
 * @return The centroid of the membership function.
 */
double calculateCentroid(MembershipFunction_t function, double membership) {
    switch (function.type) {
    case TRIANGULAR:
        return calculateTriangularCentroid(function, membership);
    case TRAPEZOIDAL:
        return calculateTrapezoidalCentroid(function, membership);
    case RECTANGULAR:
        return calculateRectangularCentroid(function, membership);
    default:
        // Handle unknown membership function type
        return 0.0;
    }
}

/**
 * Calculate the centroid of a fuzzy class.
 *
 * @param set The FuzzzySet to calculate the centroid for.
 * @return The centroid of the fuzzy class.
 */
double defuzzification(FuzzySet_t *set) {
    double sum = 0.0;
    double sumOfMemberships = 0.0;

    for (int i = 0; i < set->length; i++) {
        double membership = set->membershipValues[i];
        double x = calculateCentroid(set->membershipFunctions[i], membership);
        sum += x * membership;
        sumOfMemberships += membership;
    }

    if (sumOfMemberships == 0.0) {
        // Handle the case where the sum of memberships is zero
        // This can happen if the input is not a member of any fuzzy set
        return 0.0;
    }

    return sum / sumOfMemberships;
}
