// vsop87.cpp
//
// Copyright (C) 2002, Chris Laurel <claurel@shatters.net>
//
// Implementation of the VSOP87 theory for the the orbits of the
// major planets.  The data is a truncated version of the complete
// data set available here:
// ftp://ftp.bdl.fr/pub/ephem/planets/vsop87/
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

#include <cmath>
#include <celmath/mathlib.h>
#include <celengine/astro.h>
#include <celengine/vsop87.h>

using namespace std;


struct VSOPTerm
{
    double A, B, C;
};

struct VSOPSeries
{
    VSOPSeries(VSOPTerm* _terms, int _nTerms) :
        terms(_terms), nTerms(_nTerms) {};
    VSOPTerm* terms;
    int nTerms;
};

static VSOPTerm earth_L0[] = {
	{ 175347045.7, 0, 0 },
	{ 3341656.5, 4.66925680, 6283.07584999 },
	{ 34894.3, 4.626102, 12566.151700 },
	{ 3417.6, 2.82887, 3.52312 },
	{ 3497.1, 2.74412, 5753.38488 },
	{ 3135.9, 3.62767, 77713.77147 },
	{ 2676.2, 4.41808, 7860.41939 },
	{ 2342.7, 6.13516, 3930.20970 },
	{ 1273.2, 2.03710, 529.69097 },
	{ 1324.3, 0.74246, 11506.76977 },
	{ 901.9, 2.0451, 26.2983 },
	{ 1199.2, 1.10963, 1577.34354 },
	{ 857.2, 3.5085, 398.1490 },
	{ 779.8, 1.1788, 5223.6939 },
	{ 990.2, 5.2327, 5884.9268 },
	{ 753.1, 2.5334, 5507.5532 },
	{ 505.3, 4.5829, 18849.2275 },
	{ 492.4, 4.2051, 775.5226 },
	{ 356.7, 2.9195, 0.0673 },
	{ 284.1, 1.8987, 796.2980 },
	{ 242.8, 0.3448, 5486.7778 },
	{ 317.1, 5.8490, 11790.6291 },
	{ 271.0, 0.3149, 10977.0788 },
	{ 206.2, 4.8065, 2544.3144 },
	{ 205.4, 1.8695, 5573.1428 },
	{ 202.3, 2.4577, 6069.7768 },
	{ 126.2, 1.0830, 20.7754 },
	{ 155.5, 0.8331, 213.2991 },
	{ 115.1, 0.6454, 0.9803 },
	{ 102.9, 0.6360, 4694.0030 },
	{ 101.7, 4.2668, 7.1135 },
	{ 99.2, 6.210, 2146.165 },
	{ 132.2, 3.4112, 2942.4634 },
	{ 97.6, 0.681, 155.420 },
	{ 85.1, 1.299, 6275.962 },
	{ 74.7, 1.755, 5088.629 },
	{ 101.9, 0.9757, 15720.8388 },
	{ 84.7, 3.671, 71430.696 },
	{ 73.5, 4.679, 801.821 },
	{ 73.9, 3.503, 3154.687 },
	{ 78.8, 3.037, 12036.461 },
	{ 79.6, 1.808, 17260.155 },
	{ 85.8, 5.983, 161000.686 },
	{ 57.0, 2.784, 6286.599 },
	{ 61.1, 1.818, 7084.897 },
	{ 69.6, 0.833, 9437.763 },
	{ 56.1, 4.387, 14143.495 },
	{ 62.4, 3.978, 8827.390 },
	{ 51.1, 0.283, 5856.478 },
	{ 55.6, 3.470, 6279.553 },
	{ 41.0, 5.368, 8429.241 },
	{ 51.6, 1.333, 1748.016 },
	{ 52.0, 0.189, 12139.554 },
	{ 49.0, 0.487, 1194.447 },
	{ 39.2, 6.168, 10447.388 },
	{ 35.6, 1.776, 6812.767 },
	{ 36.8, 6.041, 10213.286 },
	{ 36.6, 2.570, 1059.382 },
	{ 33.3, 0.593, 17789.846 },
	{ 36.0, 1.709, 2352.866 },
	{ 40.9, 2.399, 19651.048 },
	{ 30.0, 2.740, 1349.867 },
	{ 30.4, 0.443, 83996.847 },
	{ 23.7, 0.485, 8031.092 },
	{ 23.6, 2.065, 3340.612 },
	{ 21.1, 4.148, 951.718 },
	{ 24.7, 0.215, 3.590 },
	{ 25.4, 3.165, 4690.480 },
	{ 22.8, 5.222, 4705.732 },
	{ 21.4, 1.426, 16730.464 },
	{ 21.9, 5.556, 553.569 },
	{ 17.5, 4.561, 135.065 },
	{ 19.9, 5.222, 12168.003 },
	{ 19.9, 5.775, 6309.374 },
	{ 20.3, 0.371, 283.859 },
	{ 14.4, 4.193, 242.729 },
	{ 16.2, 5.988, 11769.854 },
	{ 15.1, 4.196, 6256.778 },
	{ 19.1, 3.822, 23581.258 },
	{ 18.9, 5.386, 149854.400 },
	{ 14.3, 3.724, 38.028 },
	{ 17.9, 2.215, 13367.973 },
	{ 12.1, 2.622, 955.600 },
	{ 11.3, 0.177, 4164.312 },
	{ 14.0, 4.401, 6681.225 },
	{ 13.6, 1.889, 7632.943 },
	{ 12.5, 1.131, 5.523 },
	{ 10.5, 5.359, 1592.596 },
	{ 9.8, 1.00, 11371.70 },
	{ 9.2, 4.57, 4292.33 },
	{ 10.3, 6.200, 6438.496 },
	{ 12.0, 1.004, 632.784 },
	{ 10.8, 0.327, 103.093 },
	{ 8.4, 4.54, 25132.30 },
	{ 10.0, 6.029, 5746.271 },
	{ 8.4, 3.30, 7234.79 },
	{ 8.0, 5.82, 28.45 },
	{ 10.5, 0.939, 11926.254 },
	{ 7.7, 3.12, 7238.68 },
	{ 9.4, 2.62, 5760.50 },
	{ 8.1, 6.11, 4732.03 },
	{ 9.2, 0.48, 522.58 },
	{ 9.8, 5.24, 27511.47 },
	{ 7.9, 1.00, 5643.18 },
	{ 8.1, 6.27, 426.60 },
	{ 9.0, 5.34, 6386.17 },
	{ 8.6, 4.17, 7058.60 },
	{ 6.3, 4.72, 6836.65 },
	{ 7.6, 3.97, 11499.66 },
	{ 7.8, 2.96, 23013.54 },
	{ 7.3, 0.61, 11513.88 },
	{ 6.5, 5.79, 18073.70 },
	{ 7.2, 4.00, 74.78 },
	{ 7.3, 4.39, 316.39 },
	{ 7.1, 0.32, 263.08 },
	{ 6.6, 3.66, 17298.18 },
	{ 6.8, 5.91, 90955.55 },
	/* 117 terms retained, 442 terms dropped, error 0.3" */
};

static VSOPTerm earth_L1[] = {
	/* earth l, T^1 */
	{ 628331966747.5, 0, 0 },
	{ 206058.9, 2.6782346, 6283.0758500 },
	{ 4303.4, 2.63513, 12566.15170 },
	{ 425.3, 1.5905, 3.5231 },
	{ 109.0, 2.9662, 1577.3435 },
	{ 93.5, 2.592, 18849.228 },
	{ 119.3, 5.7956, 26.2983 },
	{ 72.1, 1.138, 529.691 },
	{ 67.8, 1.875, 398.149 },
	{ 67.3, 4.409, 5507.553 },
	{ 59.0, 2.888, 5223.694 },
	{ 56.0, 2.175, 155.420 },
	{ 45.4, 0.398, 796.298 },
	{ 36.4, 0.466, 775.523 },
	{ 29.0, 2.647, 7.114 },
	{ 19.1, 1.846, 5486.778 },
	{ 20.8, 5.341, 0.980 },
	{ 18.5, 4.969, 213.299 },
	{ 16.2, 0.032, 2544.314 },
	{ 17.3, 2.991, 6275.962 },
	{ 15.8, 1.430, 2146.165 },
	{ 14.6, 1.205, 10977.079 },
	{ 11.9, 3.258, 5088.629 },
	{ 11.5, 2.075, 4694.003 },
	{ 9.7, 4.24, 1349.87 },
	{ 10.0, 1.303, 6286.599 },
	{ 9.5, 2.70, 242.73 },
	{ 12.5, 2.834, 1748.016 },
	{ 11.8, 5.274, 1194.447 },
	{ 8.6, 5.64, 951.72 },
	{ 10.6, 0.766, 553.569 },
	{ 7.6, 5.30, 2352.87 },
	{ 5.8, 1.77, 1059.38 },
	{ 6.4, 2.65, 9437.76 },
	{ 5.2, 5.66, 71430.70 },
	{ 5.3, 0.91, 3154.69 },
	{ 6.1, 4.67, 4690.48 },
	{ 4.3, 0.24, 6812.77 },
	{ 5.0, 1.42, 6438.50 },
	{ 4.3, 0.77, 10447.39 },
	{ 5.2, 1.85, 801.82 },
	{ 3.7, 2.00, 8031.09 },
	{ 3.6, 2.43, 14143.50 },
	{ 3.4, 3.86, 1592.60 },
	{ 3.4, 0.89, 12036.46 },
	{ 3.2, 3.19, 4705.73 },
	{ 3.2, 0.62, 8429.24 },
	{ 4.1, 5.24, 7084.90 },
	{ 3.0, 6.07, 4292.33 },
	{ 2.9, 2.32, 20.36 },
	{ 3.5, 4.80, 6279.55 },
	{ 2.9, 1.43, 5746.27 },
	{ 2.7, 4.80, 7234.79 },
	{ 2.5, 6.22, 6836.65 },
	{ 2.7, 0.93, 5760.50 },
	{ 3.2, 3.40, 7632.94 },
	{ 2.3, 5.00, 17789.85 },
	{ 2.1, 3.96, 10213.29 },
	{ 2.1, 2.22, 5856.48 },
	{ 2.3, 5.67, 11499.66 },
	{ 2.1, 5.20, 11513.88 },
	{ 1.9, 0.53, 3340.61 },
	{ 1.9, 4.74, 83996.85 },
	{ 2.1, 2.55, 25132.30 },
	{ 1.8, 1.47, 4164.31 },
	{ 1.8, 3.02, 5.52 },
	{ 2.0, 0.91, 6256.78 },
	{ 2.1, 2.27, 522.58 },
	{ 1.8, 3.03, 5753.38 },
	{ 1.6, 6.12, 5216.58 },
	{ 1.6, 4.64, 3.29 },
	/* 71 terms retained, 270 terms dropped, error 0.056"*T */
};

static VSOPTerm earth_L2[] = {
	/* earth l, T^2 */
	{ 52918.9, 0, 0 },
	{ 8719.8, 1.07210, 6283.07585 },
	{ 309.1, 0.8673, 12566.1517 },
	{ 27.3, 0.053, 3.523 },
	{ 16.3, 5.188, 26.298 },
	{ 15.8, 3.685, 155.420 },
	{ 9.5, 0.76, 18849.23 },
	{ 8.9, 2.06, 77713.77 },
	{ 7.0, 0.83, 775.52 },
	{ 5.1, 4.66, 1577.34 },
	{ 4.1, 1.03, 7.11 },
	{ 3.5, 5.14, 796.30 },
	{ 3.2, 6.05, 5507.55 },
	{ 3.0, 1.19, 242.73 },
	{ 2.9, 6.12, 529.69 },
	{ 3.8, 3.44, 5573.14 },
	{ 2.7, 0.31, 398.15 },
	{ 2.4, 4.38, 5223.69 },
	{ 2.5, 2.28, 553.57 },
	{ 2.1, 3.75, 0.98 },
	{ 1.7, 0.90, 951.72 },
	{ 1.5, 5.76, 1349.87 },
	{ 1.2, 2.97, 2146.17 },
	{ 1.4, 4.36, 1748.02 },
	{ 1.3, 3.72, 1194.45 },
	{ 1.3, 2.95, 6438.50 },
	{ 1.0, 5.99, 6286.60 },
	{ 0.9, 4.80, 5088.63 },
	{ 0.8, 3.31, 213.30 },
	{ 1.1, 1.27, 161000.69 },
	{ 0.8, 3.42, 5486.78 },
	{ 1.0, 0.60, 3154.69 },
	{ 0.9, 5.23, 7084.90 },
	{ 0.6, 1.60, 2544.31 },
	{ 0.7, 3.43, 4694.00 },
	{ 0.6, 2.48, 10977.08 },
	{ 0.7, 6.19, 4690.48 },
	{ 0.6, 1.98, 801.82 },
	{ 0.5, 1.44, 6836.65 },
	{ 0.5, 2.34, 1592.60 },
	{ 0.5, 1.31, 4292.33 },
	{ 0.4, 0.04, 7234.79 },
	{ 0.5, 3.81, 149854.40 },
	{ 0.4, 4.94, 7632.94 },
	/* 44 terms retained, 98 terms dropped, error 0.011"*T^2 */
};

static VSOPTerm earth_L3[] = {
	/* earth l, T^3 */
	{ 289.2, 5.8438, 6283.0758 },
	{ 35.0, 0, 0 },
	{ 16.8, 5.488, 12566.152 },
	{ 3.0, 5.20, 155.42 },
	{ 1.3, 4.72, 3.52 },
	{ 0.6, 5.97, 242.73 },
	{ 0.7, 5.30, 18849.23 },
	{ 0.4, 3.79, 553.57 },
	/* 8 terms retained, 14 terms dropped, error 0.005"*T^3 */
};

static VSOPTerm earth_L4[] = {
	/* earth l, T^4 */
	{ 114.1, 3.1416, 0 },
	{ 7.7, 4.13, 6283.08 },
	{ 0.8, 3.84, 12566.15 },
	{ 0.4, 0.42, 155.42 },
	/* 4 terms retained, 7 terms dropped, error 0.00032"*T^4 */
};

static VSOPTerm earth_L5[] = {
	/* earth l, T^5 */
	{ 0.9, 3.14, 0 },
	{ 0.2, 2.77, 6283.08 },
	{ 0.1, 2.01, 155.42 },
	/* 3 terms retained, 2 terms dropped, error 0.00023"*T^5 */
	/* end earth l */
};

static VSOPTerm earth_B0[] = {
	/* earth b, T^0 */
	{ 279.6, 3.1987, 84334.6616 },
	{ 101.6, 5.4225, 5507.5532 },
	{ 80.4, 3.880, 5223.694 },
	{ 43.8, 3.704, 2352.866 },
	{ 31.9, 4.000, 1577.344 },
	{ 22.7, 3.985, 1047.747 },
	{ 16.4, 3.565, 5856.478 },
	{ 18.1, 4.984, 6283.076 },
	{ 14.4, 3.703, 9437.763 },
	{ 14.3, 3.411, 10213.286 },
	{ 11.2, 4.828, 14143.495 },
	{ 10.9, 2.086, 6812.767 },
	{ 9.7, 3.47, 4694.00 },
	{ 10.4, 4.057, 71092.881 },
	{ 8.8, 4.44, 5753.38 },
	{ 8.4, 4.99, 7084.90 },
	{ 6.9, 4.33, 6275.96 },
	{ 9.1, 1.14, 6620.89 },
	{ 7.2, 3.60, 529.69 },
	{ 7.7, 5.55, 167621.58 },
	/* 20 terms retained, 164 terms dropped, error 0.15" */
};

static VSOPTerm earth_B1[] = {
	/* earth b, T^1 */
	{ 9.0, 3.90, 5507.55 },
	{ 6.2, 1.73, 5223.69 },
	{ 3.8, 5.24, 2352.87 },
	{ 2.8, 2.47, 1577.34 },
	{ 1.8, 0.42, 6283.08 },
	/* 5 terms retained, 94 terms dropped, error 0.018"*T */
};

static VSOPTerm earth_B2[] = {
	/* earth b, T^2 */
	{ 1.7, 1.63, 84334.66 },
	{ 0.5, 2.41, 1047.75 },
	/* 2 terms retained, 47 terms dropped, error 0.0035"*T^2 */
};

	/* earth b, T^3 */
	/* 0 terms retained, 11 terms dropped, error 4.5e-05"*T^3 */

	/* earth b, T^4 */
	/* 0 terms retained, 5 terms dropped, error 1.7e-05"*T^4 */
	/* end earth b */

static VSOPTerm earth_R0[] = {
	/* earth r, T^0 */
	{ 100013988.8, 0, 0 },
	{ 1670699.6, 3.09846351, 6283.07584999 },
	{ 13956.0, 3.055246, 12566.151700 },
	{ 3083.7, 5.19847, 77713.77147 },
	{ 1628.5, 1.17388, 5753.38488 },
	{ 1575.6, 2.84685, 7860.41939 },
	{ 924.8, 5.4529, 11506.7698 },
	{ 542.4, 4.5641, 3930.2097 },
	{ 472.1, 3.6610, 5884.9268 },
	{ 328.8, 5.8998, 5223.6939 },
	{ 346.0, 0.9637, 5507.5532 },
	{ 306.8, 0.2987, 5573.1428 },
	{ 174.8, 3.0119, 18849.2275 },
	{ 243.2, 4.2735, 11790.6291 },
	{ 211.8, 5.8471, 1577.3435 },
	{ 185.8, 5.0219, 10977.0788 },
	{ 109.8, 5.0551, 5486.7778 },
	{ 98.3, 0.887, 6069.777 },
	{ 86.5, 5.690, 15720.839 },
	{ 85.8, 1.271, 161000.686 },
	{ 62.9, 0.922, 529.691 },
	{ 57.1, 2.014, 83996.847 },
	{ 64.9, 0.273, 17260.155 },
	{ 49.4, 3.245, 2544.314 },
	{ 55.7, 5.242, 71430.696 },
	{ 42.5, 6.011, 6275.962 },
	{ 47.0, 2.578, 775.523 },
	{ 39.0, 5.361, 4694.003 },
	{ 44.7, 5.537, 9437.763 },
	{ 35.7, 1.675, 12036.461 },
	{ 31.9, 0.184, 5088.629 },
	{ 31.8, 1.778, 398.149 },
	{ 33.2, 0.244, 7084.897 },
	{ 38.2, 2.393, 8827.390 },
	{ 28.5, 1.213, 6286.599 },
	{ 37.5, 0.830, 19651.048 },
	{ 37.0, 4.901, 12139.554 },
	{ 34.5, 1.843, 2942.463 },
	{ 26.3, 4.589, 10447.388 },
	{ 24.6, 3.787, 8429.241 },
	{ 23.6, 0.269, 796.298 },
	{ 27.8, 1.899, 6279.553 },
	{ 23.9, 4.996, 5856.478 },
	{ 20.3, 4.653, 2146.165 },
	{ 23.3, 2.808, 14143.495 },
	{ 22.1, 1.950, 3154.687 },
	{ 19.5, 5.382, 2352.866 },
	{ 18.0, 0.199, 6812.767 },
	{ 17.2, 4.433, 10213.286 },
	{ 16.2, 5.232, 17789.846 },
	{ 17.3, 6.152, 16730.464 },
	{ 13.8, 5.190, 8031.092 },
	{ 18.8, 0.673, 149854.400 },
	{ 18.3, 2.253, 23581.258 },
	{ 13.6, 3.685, 4705.732 },
	{ 13.1, 0.653, 13367.973 },
	{ 10.4, 4.333, 11769.854 },
	{ 10.0, 4.201, 6309.374 },
	{ 10.2, 1.594, 4690.480 },
	{ 7.6, 2.63, 6256.78 },
	{ 9.7, 3.68, 27511.47 },
	{ 6.7, 0.56, 3340.61 },
	{ 8.7, 6.06, 1748.02 },
	{ 7.8, 3.67, 12168.00 },
	{ 6.6, 5.66, 11371.70 },
	{ 7.7, 0.31, 7632.94 },
	{ 6.6, 3.14, 801.82 },
	{ 7.5, 5.65, 11926.25 },
	{ 6.9, 2.92, 6681.22 },
	{ 6.8, 1.42, 23013.54 },
	{ 6.5, 2.65, 19804.83 },
	/* 71 terms retained, 455 terms dropped, error 1.1e-06 a.u. */
};

static VSOPTerm earth_R1[] = {
	/* earth r, T^1 */
	{ 103018.6, 1.1074897, 6283.0758500 },
	{ 1721.2, 1.06442, 12566.15170 },
	{ 702.2, 3.1416, 0 },
	{ 32.3, 1.022, 18849.228 },
	{ 30.8, 2.844, 5507.553 },
	{ 25.0, 1.319, 5223.694 },
	{ 18.5, 1.424, 1577.344 },
	{ 10.1, 5.914, 10977.079 },
	{ 8.6, 0.27, 5486.78 },
	{ 8.7, 1.42, 6275.96 },
	{ 5.1, 1.69, 5088.63 },
	{ 5.0, 6.01, 6286.60 },
	{ 4.7, 5.99, 529.69 },
	{ 4.4, 0.52, 4694.00 },
	{ 3.9, 4.75, 2544.31 },
	{ 3.8, 5.07, 796.30 },
	{ 4.1, 1.08, 9437.76 },
	{ 3.5, 0.02, 83996.85 },
	{ 3.4, 0.95, 71430.70 },
	{ 3.2, 6.16, 2146.17 },
	{ 3.4, 5.41, 775.52 },
	{ 2.9, 5.48, 10447.39 },
	{ 2.5, 0.24, 398.15 },
	{ 2.2, 4.95, 6812.77 },
	{ 2.2, 0.42, 8031.09 },
	{ 2.8, 3.42, 2352.87 },
	{ 2.6, 6.13, 6438.50 },
	{ 1.9, 5.31, 8429.24 },
	{ 2.4, 3.09, 4690.48 },
	{ 1.7, 1.54, 4705.73 },
	{ 2.2, 3.69, 7084.90 },
	{ 2.1, 1.28, 1748.02 },
	{ 1.8, 3.23, 6279.55 },
	{ 1.6, 4.10, 11499.66 },
	{ 1.6, 5.54, 3154.69 },
	{ 1.8, 1.82, 7632.94 },
	/* 36 terms retained, 256 terms dropped, error 2.2e-07 a.u.*T */
};

static VSOPTerm earth_R2[] = {
	/* earth r, T^2 */
	{ 4359.4, 5.78455, 6283.07585 },
	{ 123.6, 5.5793, 12566.1517 },
	{ 12.3, 3.142, 0 },
	{ 8.8, 3.63, 77713.77 },
	{ 5.7, 1.87, 5573.14 },
	{ 3.3, 5.47, 18849.23 },
	{ 1.5, 4.48, 5507.55 },
	{ 1.0, 2.81, 5223.69 },
	{ 0.9, 3.11, 1577.34 },
	{ 1.1, 2.84, 161000.69 },
	{ 0.6, 5.47, 775.52 },
	{ 0.6, 1.38, 6438.50 },
	{ 0.5, 4.42, 6286.60 },
	{ 0.4, 0.90, 10977.08 },
	{ 0.4, 3.20, 5088.63 },
	{ 0.5, 3.66, 7084.90 },
	{ 0.5, 5.39, 149854.40 },
	/* 17 terms retained, 122 terms dropped, error 3.9e-08 a.u.*T^2 */
};

static VSOPTerm earth_R3[] = {
	/* earth r, T^3 */
	{ 144.6, 4.2732, 6283.0758 },
	{ 6.7, 3.92, 12566.15 },
	{ 0.8, 0, 0 },
	{ 0.2, 3.73, 18849.23 },
	/* 4 terms retained, 23 terms dropped, error 1.1e-08 a.u.*T^3 */
};

static VSOPTerm earth_R4[] = {
	/* earth r, T^4 */
	{ 3.9, 2.56, 6283.08 },
	{ 0.3, 2.27, 12566.15 },
	{ 0.1, 3.44, 5573.14 },
	/* 3 terms retained, 7 terms dropped, error 2.1e-09 a.u.*T^4 */
};

static VSOPTerm earth_R5[] = {
	/* earth r, T^5 */
	{ 0.1, 1.22, 6283.08 },
	/* 1 terms retained, 2 terms dropped, error 2.4e-09 a.u.*T^5 */
	/* end earth */
};

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))
#define VSOP_SERIES(s) VSOPSeries(s, ARRAY_SIZE(s))

static VSOPSeries earth_L[] = {
    VSOP_SERIES(earth_L0), VSOP_SERIES(earth_L1), VSOP_SERIES(earth_L2),
    VSOP_SERIES(earth_L3), VSOP_SERIES(earth_L4), VSOP_SERIES(earth_L5),
};

static VSOPSeries earth_B[] = {
    VSOP_SERIES(earth_B0), VSOP_SERIES(earth_B1), VSOP_SERIES(earth_B2),
};

static VSOPSeries earth_R[] = {
    VSOP_SERIES(earth_R0), VSOP_SERIES(earth_R1), VSOP_SERIES(earth_R2),
    VSOP_SERIES(earth_R3), VSOP_SERIES(earth_R4), VSOP_SERIES(earth_R5),
};

static double SumSeries(const VSOPSeries& series, double t)
{
    if (series.nTerms < 1)
        return 0.0;

    double x = 0.0;
    VSOPTerm* term = &series.terms[0];
    for (int i = 0; i < series.nTerms; i++, term++)
        x += term->A * cos(term->B + term->C * t);

    return x;
};

class VSOP87Orbit : public CachingOrbit
{
private:
    VSOPSeries* L;
    VSOPSeries* B;
    VSOPSeries* R;
    int nL;
    int nB;
    int nR;
    double period;
    double boundingRadius;

public:
    VSOP87Orbit(VSOPSeries* _L, int _nL,
                VSOPSeries* _B, int _nB,
                VSOPSeries* _R, int _nR,
                double _period,
                double _boundingRadius) :
        L(_L), nL(_nL),
        B(_B), nB(_nB),
        R(_R), nR(_nR),
        period(_period),
        boundingRadius(_boundingRadius)
    {
    };

    double getPeriod() const
    {
        return period;
    }

    double getBoundingRadius() const
    {
        return boundingRadius;
    }

    Point3d computePosition(double jd) const
    {
        // t is Julian millenia since J2000.0
        double t = (jd - 2451545.0) / 365250.0;

        // Heliocentric coordinates
        double l = 0.0; // longitude
        double b = 0.0; // latitude
        double r = 0.0; // radius
        
        int i;
        double T;;

        // Evaluate series for l
        T = 1;
        for (i = 0; i < nL; i++)
        {
            l += SumSeries(L[i], t) * T;
            T = t * T;
        }

        // Evaluate series for b
        T = 1;
        for (i = 0; i < nB; i++)
        {
            b += SumSeries(B[i], t) * T;
            T = t * T;
        }

        // Evaluate series for r
        T = 1;
        for (i = 0; i < nR; i++)
        {
            r += SumSeries(R[i], t) * T;
            T = t * T;
        }
        
        // l and b are in units of 1e+8 radians
        l *= 1.0e-8;
        b *= 1.0e-8;
        // r is in units of 1e-8 AU
        r *= (KM_PER_AU / 100000000.0);

        // Corrections for internal coordinate system
        b -= PI / 2;
        l += PI;

        return Point3d(cos(l) * sin(b) * r,
                       cos(b) * r,
                       -sin(l) * sin(b) * r);
    }
};


Orbit* CreateVSOP87Orbit(const string& name)
{
    if (name == "vsop87-earth")
    {
        return new VSOP87Orbit(earth_L, 6,
                               earth_B, 3,
                               earth_R, 6,
                               365.25,
                               160000000.0);
    }

    return NULL;
}
