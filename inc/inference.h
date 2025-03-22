

 #include "class.h"
 #include "classifier.h"
 #include "membership_function.h"
 
 #include <stdbool.h>
 
 #ifndef FUZZY_INFERENCE_H
 #define FUZZY_INFERENCE_H
 #pragma once
 
 // Define a type for a fuzzy variable
 typedef struct {
     FuzzySet_t *variable;
     int value;
     bool invert;
 } FuzzyVariable_t;
 
 // Define a type for a fuzzy antecedent
 typedef enum { FUZZY_ANY_OF, FUZZY_ALL_OF } Fuzzyfuzzy_operator_e;
 
 typedef struct {
     FuzzyVariable_t *variables;
     int num_variables;
     Fuzzyfuzzy_operator_e fuzzy_operator;
 } FuzzyAntecedent_t;
 
 // Define a type for a fuzzy rule
 typedef struct {
     FuzzyAntecedent_t *antecedent;
     int num_antecedents;
     FuzzyVariable_t *consequents;  // Mảng các kết quả đầu ra
     int num_consequents;           // Số lượng kết quả đầu ra
 } FuzzyRule_t;
 
 // Define macros to create fuzzy variables and antecedents
 #define NOT(_variable, _value)                                                 \
     (FuzzyVariable_t) {                                                        \
         .variable = &_variable, .value = _value, .invert = true                \
     }
 
 #define VAR(_variable, _value)                                                 \
     (FuzzyVariable_t) {                                                        \
         .variable = &_variable, .value = _value, .invert = false               \
     }
 
 #define THEN(_variable, _value)                                                \
     (FuzzyVariable_t) { .variable = &_variable, .value = _value }
 
 #define ANY_OF(...)                                                            \
     {.fuzzy_operator= FUZZY_ANY_OF,                                                  \
      .variables = (FuzzyVariable_t[]){__VA_ARGS__},                            \
      .num_variables =                                                          \
          sizeof((FuzzyVariable_t[]){__VA_ARGS__}) / sizeof(FuzzyVariable_t)}
 
 #define ALL_OF(...)                                                            \
     {.fuzzy_operator= FUZZY_ALL_OF,                                                  \
      .variables = (FuzzyVariable_t[]){__VA_ARGS__},                            \
      .num_variables =                                                          \
          sizeof((FuzzyVariable_t[]){__VA_ARGS__}) / sizeof(FuzzyVariable_t)}
 
 #define WHEN(...) {__VA_ARGS__}
 
 // Define a macro to create a fuzzy rule
 #define PROPOSITION(_antecedent, ...)                                          \
     {.antecedent = (FuzzyAntecedent_t[])_antecedent,                           \
      .num_antecedents =                                                        \
          sizeof((FuzzyAntecedent_t[])_antecedent) / sizeof(FuzzyAntecedent_t), \
      .consequents = (FuzzyVariable_t[]){__VA_ARGS__},                          \
      .num_consequents = sizeof((FuzzyVariable_t[]){__VA_ARGS__}) / sizeof(FuzzyVariable_t)}
 
 
 void fuzzyInference(const FuzzyRule_t *rules, int numRules);
 
 #endif