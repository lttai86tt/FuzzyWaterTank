#include "class.h"

#include "membership_function.h"
#include "defuzzifier.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

/**
 * Initializes a FuzzySet_t struct.
 *
 * This function allocates memory for the membership values in the FuzzySet_t
 * struct.
 *
 * @param set The FuzzySet_t struct to initialize.
 * @param membershipFunctions The membership functions for this FuzzySet_t.
 * @param length The number of membership values and Functions in the set.
 */
void FuzzySetInit(FuzzySet_t *set,
                  const MembershipFunction_t *membershipFunctions, int length) {

    set->length = length;

    set->membershipValues = (double *)malloc(length * sizeof(double));
    set->membershipFunctions =
        (MembershipFunction_t *)malloc(length * sizeof(MembershipFunction_t));

    for (int i = 0; i < length; i++) {
        set->membershipFunctions[i] = membershipFunctions[i];
    }
}

/**
 * Frees the memory allocated for a FuzzySet_t struct.
 *
 * This function should be called when the FuzzySet_t struct is no longer
 * needed.
 *
 * @param set The FuzzySet_t struct to free.
 */
void FuzzySetFree(FuzzySet_t *set) {
    free(set->membershipValues);
    free(set->membershipFunctions);
}

/**
 * Returns the maximum possible value of a FuzzySet_t struct.
 *
 * @param set The FuzzySet_t struct to evaluate.
 */
double getMaxOutput(FuzzySet_t *set)    {
    double max = -INFINITY;

    for (int i = 0; i < set->length; i++) {
        double membershipDegree = calculateCentroid(set->membershipFunctions[i], 1.0);
        if (membershipDegree > max) {
            max = membershipDegree;
        }
    }

    return max;
}

/**
 * Returns the minimum possible value of a FuzzySet_t struct.
 *
 * @param set The FuzzySet_t struct to evaluate.
 */
double getMinOutput(FuzzySet_t *set)    {
    double min = INFINITY;

    for (int i = 0; i < set->length; i++) {
        double membershipDegree = calculateCentroid(set->membershipFunctions[i], 1.0);
        if (membershipDegree < min) {
            min = membershipDegree;
        }
    }

    return min;
}

/**
 * Normalizes the membership values in a FuzzySet_t struct.
 *
 * This function calculates the sum of all membership values and divides each
 * membership value by the sum.
 *
 * @param set The FuzzySet_t struct to normalize.
 */
void normalizeClass(FuzzySet_t *set) {
    // Calculate the sum of all membership values
    double sum = 0.0;
    for (int i = 0; i < set->length; i++) {
        sum += set->membershipValues[i];
    }

    // Check for division by zero
    if (sum == 0.0) {
        // Handle the case where the sum is zero
        // For example, set all membership values to 0.0
        for (int i = 0; i < set->length; i++) {
            set->membershipValues[i] = 0.0;
        }
    } else {
        // Normalize the membership values
        for (int i = 0; i < set->length; i++) {
            set->membershipValues[i] /= sum;
        }
    }
}

/**
 * Prints the classification results in a FuzzySet_t struct.
 *
 * This function prints the membership values in a FuzzySet_t struct along with
 * a bar chart representation.
 *
 * @param set The FuzzySet_t struct to print.
 * @param labels The array of labels to use for the membership values.
 */
void printClassifier(FuzzySet_t *set, const char **labels) {
    for (int i = 0; i < set->length; i++) {
        printf("%s", labels[i]);

        printf("\t [");
        const int bar_length = 24;
        int threshold = round(set->membershipValues[i] * bar_length) - 1;
        for (int i = 0; i < bar_length; i++) {
            if (i < threshold) {
                printf("=");
            } else if (i == threshold) {
                printf(">");
            } else {
                printf(" ");
            }
        }
        printf("] %6.2f %%\n", set->membershipValues[i] * 100.0);
    }
    printf("\n");
}