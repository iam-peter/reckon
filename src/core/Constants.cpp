#include "Constants.h"

using namespace Qt::StringLiterals;

// CODATA 2018 values, SI units.
const QList<ConstantInfo> &constants()
{
    static const QList<ConstantInfo> list = {
        {u"Pi"_s, 3.14159265358979323846, u""_s, u"Mathematics"_s},
        {u"Euler's number"_s, 2.71828182845904523536, u""_s, u"Mathematics"_s},
        {u"Golden ratio"_s, 1.61803398874989484820, u""_s, u"Mathematics"_s},
        {u"Euler–Mascheroni constant"_s, 0.57721566490153286061, u""_s, u"Mathematics"_s},
        {u"Square root of 2"_s, 1.41421356237309504880, u""_s, u"Mathematics"_s},

        {u"Speed of light in vacuum"_s, 299792458.0, u"m/s"_s, u"Universal"_s},
        {u"Gravitational constant"_s, 6.67430e-11, u"m³/(kg·s²)"_s, u"Universal"_s},
        {u"Planck constant"_s, 6.62607015e-34, u"J·s"_s, u"Universal"_s},
        {u"Reduced Planck constant"_s, 1.054571817e-34, u"J·s"_s, u"Universal"_s},
        {u"Standard gravity"_s, 9.80665, u"m/s²"_s, u"Universal"_s},
        {u"Standard atmosphere"_s, 101325.0, u"Pa"_s, u"Universal"_s},

        {u"Elementary charge"_s, 1.602176634e-19, u"C"_s, u"Electromagnetic"_s},
        {u"Vacuum permittivity"_s, 8.8541878128e-12, u"F/m"_s, u"Electromagnetic"_s},
        {u"Vacuum permeability"_s, 1.25663706212e-6, u"N/A²"_s, u"Electromagnetic"_s},
        {u"Coulomb constant"_s, 8.9875517923e9, u"N·m²/C²"_s, u"Electromagnetic"_s},
        {u"Magnetic flux quantum"_s, 2.067833848e-15, u"Wb"_s, u"Electromagnetic"_s},
        {u"Bohr magneton"_s, 9.2740100783e-24, u"J/T"_s, u"Electromagnetic"_s},

        {u"Electron mass"_s, 9.1093837015e-31, u"kg"_s, u"Atomic and nuclear"_s},
        {u"Proton mass"_s, 1.67262192369e-27, u"kg"_s, u"Atomic and nuclear"_s},
        {u"Neutron mass"_s, 1.67492749804e-27, u"kg"_s, u"Atomic and nuclear"_s},
        {u"Atomic mass unit"_s, 1.66053906660e-27, u"kg"_s, u"Atomic and nuclear"_s},
        {u"Bohr radius"_s, 5.29177210903e-11, u"m"_s, u"Atomic and nuclear"_s},
        {u"Fine-structure constant"_s, 7.2973525693e-3, u""_s, u"Atomic and nuclear"_s},
        {u"Rydberg constant"_s, 10973731.568160, u"1/m"_s, u"Atomic and nuclear"_s},
        {u"Classical electron radius"_s, 2.8179403262e-15, u"m"_s, u"Atomic and nuclear"_s},

        {u"Avogadro constant"_s, 6.02214076e23, u"1/mol"_s, u"Physico-chemical"_s},
        {u"Boltzmann constant"_s, 1.380649e-23, u"J/K"_s, u"Physico-chemical"_s},
        {u"Gas constant"_s, 8.314462618, u"J/(mol·K)"_s, u"Physico-chemical"_s},
        {u"Faraday constant"_s, 96485.33212, u"C/mol"_s, u"Physico-chemical"_s},
        {u"Stefan–Boltzmann constant"_s, 5.670374419e-8, u"W/(m²·K⁴)"_s, u"Physico-chemical"_s},
        {u"Molar volume of ideal gas (0 °C, 1 atm)"_s, 22.41396954e-3, u"m³/mol"_s, u"Physico-chemical"_s},

        {u"Astronomical unit"_s, 1.495978707e11, u"m"_s, u"Astronomy"_s},
        {u"Light year"_s, 9.4607304725808e15, u"m"_s, u"Astronomy"_s},
        {u"Parsec"_s, 3.0856775814913673e16, u"m"_s, u"Astronomy"_s},
        {u"Solar mass"_s, 1.98892e30, u"kg"_s, u"Astronomy"_s},
        {u"Earth mass"_s, 5.9722e24, u"kg"_s, u"Astronomy"_s},
        {u"Earth radius (mean)"_s, 6371008.8, u"m"_s, u"Astronomy"_s},
        {u"Moon mass"_s, 7.342e22, u"kg"_s, u"Astronomy"_s},
    };
    return list;
}
