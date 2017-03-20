#include "NuclearStructureManager"
#include "OptionContainer.h"
#include "Constants.h"
#include "MatrixElements.h"

#include "boost/algorithm/string.hpp"
#include "gsl/gsl_sf_coupling.h"

namespace NS = NuclearStructure;
namespace NO = NuclearStructure::nilsson;
namespace ME = NuclearStructure::MatrixElements;

int sign(double x) { return (x >= 0.) ? 1 : -1; }

NS::NuclearStructureManager::NuclearStructureManager(BetaType bt,
                                                     NS::Nucleus init,
                                                     NS::Nucleus fin) {
  betaType = bt;
  mother = init;
  daughter = fin;
}

void NS::NuclearStructureManager::SetDaughterNucleus(int Z, int A, int dJ,
                                                     double R,
                                                     double excitationEnergy,
                                                     double beta2,
                                                     double beta4) {
  daughter = {Z, A, dJ, R, excitationEnergy, beta2, beta4};
}

void NS::NuclearStructureManager::SetMotherNucleus(int Z, int A, int dJ,
                                                   double R,
                                                   double excitationEnergy,
                                                   double beta2, double beta4) {
  mother = {Z, A, dJ, R, excitationEnergy, beta2, beta4};
}

void NS::NuclearStructureManager::Initialize(std::string method,
                                             std::string pot) {
  // Extreme Single particle
  if (boost::iequals(method, "ESP")) {
    SingleParticleState spsi, spsf;
    int dKi, dKf;
    double obdme;
    GetESPStates(spsi, spsf, pot, dKi, dKf, obdme);
    AddOneBodyTransition(obdme, dKi, dKf, spsi, spsf);
  }
}

void NS::NuclearStructureManager::GetESPStates(SingleParticleState& spsi,
                                               SingleParticleState& spsf,
                                               std::string pot, int& dKi,
                                               int& dKfi, double obdme) {
  double V0p = GetOpt(double, Computational.V0proton);
  double V0n = GetOpt(double, Computational.V0neutron);
  double A0 = GetOpt(double, Computational.SurfaceThickness);
  double VSp = GetOpt(double, Computational.V0Sproton);
  double VSn = GetOpt(double, Computational.V0Sneutron);

  double mR = mother.R * NATLENGTH * 1e15;
  double dR = daughter.R * NATLENGTH * 1e15;

  if (boost::iequals(pot, "SHO")) {
    int ni, li, si, nf, lf, sf;
    GetESPOrbitalNumbers(ni, li, si, nf, lf, sf);
    WFComp fW = {1.0, nf, lf, sf};
    WFComp iW = {1.0, ni, li, si};
    std::vector<WFComp> fComps = {fW};
    std::vector<WFComp> iComps = {iW};
    spsf = {daughter.dJ, -1, sign(daughter.dJ), 0.0, fComps};
    spsi = {mother.dJ, -1, sign(mother.dJ), 0.0, iComps};
  } else {
    double dBeta2 = 0.0, dBeta4 = 0.0, mBeta2 = 0.0, mBeta4 = 0.0;
    if (boost::iequals(pot, "DWS")) {
      dBeta2 = daughter.Beta2;
      dBeta4 = daughter.Beta4;
      mBeta2 = mother.Beta2;
      mBeta4 = mother.Beta4;
    }
    if (betaType == BETA_MINUS) {
      spsf = NO::CalculateDeformedSPState(daughter.Z, 0, daughter.A, daughter.dJ,
                                        dR, dBeta2, dBeta4, V0p, A0, VSp);
      spsi = NO::CalculateDeformedSPState(0, mother.A - mother.Z, mother.A,
                                             mother.dJ, mR, mBeta2, mBeta4, V0n,
                                             A0, VSn);
    } else {
      spsf = NO::CalculateDeformedSPState(0, daughter.A - daughter.Z,
                                             daughter.A, daughter.dJ, dR,
                                             dBeta2, dBeta4, V0n, A0, VSn);
      spsi = NO::CalculateDeformedSPState(mother.Z, 0, mother.A, mother.dJ,
                                             mR, mBeta2, mBeta4, V0p, A0, VSp);
    }
  }

  // Set Omega quantum numbers
  if (mother.A % 2 == 0) {
    int dTi, dTf;
    int dT3i = std::abs(mother.A - 2 * mother.Z);
    int dT3f = std::abs(daughter.A - 2 * daughter.Z);
    if ((mother.dJ + dT3i) / 2 % 2 == 0) {
      dTi = dT3i + 1;
    }
    if ((daughter.dJ + dT3f) / 2 % 2 == 0) {
      dTf = Dt3f + 1;
    }
    // TODO Implement Gallaghar coupling rules
    // Even-Even ---> Odd-Odd transition
    if (mother.Z % 2 == 0) {
      dKi = 0;
      dKf = spsi.dO + spsf.dO;
      // Deformed transition
      if (boost::iequals(pot, "DWS") && mother.Beta2 != 0.0 &&
          daughter.Beta2 != 0.0) {
        obdme = 0.5 * std::sqrt((mother.dJ + 1.) * (daughter.dJ + 1.) / 2. /
                                (1. + delta(dKf, 0.0))) *
                (1 + std::pow(-1., (dKf + spsi.dO + spsf.dO) / 2.));
        // Spherical transition
      } else {
        obdme =
            std::sqrt((mother.dJ + 1.) * (daughter.dJ + 1.) * (dTi + 1.) *
                      (dTf + 1.) / 2. /
                      (1. + delta(2 * spsi.l + spsi.s, 2 * spsf.l + spsf.s))) *
            std::pow(-1., (dTf - dT3f) / 2.) *
            gsl_sf_coupling_3j(dTf, 2, dTi, dT3f, 2 * betaType, dT3i) *
            gsl_sf_coupling_6j(1, dTf, 1, dTi, 1, 2) * std::sqrt(3. / 2.) * 2 *
            (delta(2 * spsi.l + spsi.s, 2 * spsf.l + spsf.s) -
             std::pow(-1., spsi.l + spsi.s / 2. + spsf.l + spsf.s / 2.));
      }
    //Odd-Odd ---> Even-Even transition
    } else {
      dKf = 0;
      dKi = spsi.dO + spsf.dO;
      if (boost::iequals(pot, "DWS") && mother.Beta2 != 0.0 &&
          daughter.Beta2 != 0.0) {
        obdme = 0.5 * std::sqrt((mother.dJ + 1.) * (daughter.dJ + 1.) / 2. /
                                (1. + delta(dKi, 0.0))) *
                (1 + std::pow(-1., daughter.dJ / 2.));
      } else {
        obdme =
            std::sqrt((mother.dJ + 1.) * (daughter.dJ + 1.) * (dTi + 1.) *
                      (dTf + 1.) / 2. /
                      (1. + delta(2 * spsi.l + spsi.s, 2 * spsf.l + spsf.s))) *
            std::pow(-1., (dTf - dT3f) / 2.) *
            gsl_sf_coupling_3j(dTf, 2, dTi, dT3f, 2 * betaType, dT3i) *
            gsl_sf_coupling_6j(1, dTf, 1, dTi, 1, 2) * std::sqrt(3. / 2.) * 2 *
            (1 + delta(2 * spsi.l + spsi.s, 2 * spsf.l + spsf.s));
      }
    }
  } else {
    dKi = spsi.dO;
    dKf = spsf.dO;
    if (boost::iequals(pot, "DWS") && mother.Beta2 != 0.0 &&
        daughter.Beta2 != 0.0) {
      obdme = std::sqrt((mother.dJ + 1.) * (daughter.dJ + 1.) /
                        (1. + delta(dKi, 0)) / (1. + delta(dKf, 0)));
    } else {
      // TODO check
      obdme = std::sqrt(4. * M_PI / (mother.dJ + 1.));
    }
  }
}

void NS::NuclearStructureManager::GetESPOrbitalNumbers(int& ni, int& li,
                                                       int& si, int& nf,
                                                       int& lf, int& sf) {
  std::vector<int> occNumbersInit, occNumbersFinal;
  if (betaType == BETA_MINUS) {
    occNumbersInit = utilities::GetOccupationNumbers(mother.A - mother.Z);
    occNumbersFinal = utilities::GetOccupationNumbers(daughter.Z);
  } else {
    occNumbersInit = utilities::GetOccupationNumbers(mother.Z);
    occNumbersFinal = utilities::GetOccupationNumbers(daughter.A - daughter.Z);
  }
  ni = occNumbersInit[occNumbersInit.size() - 1 - 3];
  li = occNumbersInit[occNumbersInit.size() - 1 - 2];
  si = occNumbersInit[occNumbersInit.size() - 1 - 1];
  nf = occNumbersFinal[occNumbersFinal.size() - 1 - 3];
  lf = occNumbersFinal[occNumbersFinal.size() - 1 - 2];
  sf = occNumbersFinal[occNumbersFinal.size() - 1 - 1];
}

double NS::NuclearStructureManager::CalculateMatrixElement(bool V, int K, int L,
                                                           int s) {
  double result = 0.0;
  double nu = CD::CalcNu(mother.R * std::sqrt(3. / 5.), mother.Z);

  for (int i = 0; i < oneBodyTransitions.size(); i++) {
    OneBodyTransition obt = oneBodyTransitions[i];
    result += obt.obdme * ME::GetSingleParticleMatrixElement(
                              V, std::abs(mother.dJ) / 2., K, L, s, obt.spsi,
                              obt.spsf, mother.R, nu);
  }
  return result;
}

double NS::NuclearStructureManager::CalculateWeakMagnetism() {
  double result = 0.0;

  double VM111 = CalculateMatrixElement(true, 1, 1, 1);
  double AM101 = CalculateMatrixElement(false, 1, 0, 1);

  result = -std::sqrt(2. / 3.) * nucleonMasskeV / electronMasskeV * mother.R /
               gA * VM111 / AM101 +
           gM / gA;
  return result;
}

double NuclearStructureManager::CalculateInducedTensor() {
  double result = 0.0;

  double AM110 = CalculateMatrixElement(false, 1, 1, 0);
  double AM101 = CalculateMatrixElement(false, 1, 0, 1);

  result = 2. / std::sqrt(3.) * nucleonMasskeV / electronMasskeV * mother.R *
           AM110 / AM101;
  return result;
}
  /*if (boost::iequals(pot, "SHO")) {
    int ni, li, si, nf, lf, sf;
    GetSPOrbitalNumbers(ni, li, si, nf, lf, sf);
    double dE = 2. * nu / nucleonMasskeV * (2 * (ni - nf) + li - lf);
    double rmsHO = CD::GetRMSHO(ni, li, nu);
    if (li == lf) {
      if (si == sf && si > 0) {
        return -nucleonMasskeV * dE / (2. * li + 3.) * rmsHO;
      } else if (si == sf) {
        return nucleonMasskeV * dE / (2. * li - 1.) * rmsHO;
      } else {
        return -sf * (2. * li + 1.) / 2. - dE / 2. * rmsHO;
      }
    }
  } else if (boost::iequals(pot, "WS")) {
    result =
        2. / std::sqrt(3.) * nucleonMasskeV / electronMasskeV * R *
        ME::GetSingleParticleMatrixElement(
            false, std::abs(motherSpinParity) / 2., 1, 1, 0, spsi, spsf, R, nu)
  / ME::GetSingleParticleMatrixElement(false, std::abs(motherSpinParity), 1, 0,
  1, spsi, spsf, R, nu);
  } else if (boost::iequals(pot, "DWS")) {
    result =
        2. / std::sqrt(3.) * nucleonMasskeV / electronMasskeV * R *
        ME::CalculateDeformedSPMatrixElement(spsi, spsf, false, 1, 1, 0,
  spsi.dO,
                                             spsf.dO, spsi.dK, spsf.dK, R, nu) /
        ME::CalculateDeformedSPMatrixElement(spsi, spsf, false, 1, 0, 1,
  spsi.dO,
                                             spsf.dO, spsi.dK, spsf.dK, R, nu);
  }
  return result;
}

double NuclearStructureManager::CalculateRatioM121() {
  double M121, M101;

  std::string pot = GetOpt(std::string, Computational.Potential);
  cout << "Potential: " << pot << endl;
  double nu = CD::CalcNu(R * std::sqrt(3. / 5.), Z);
  if (boost::iequals(pot, "SHO")) {
    int ni, li, si, nf, lf, sf;
    GetSPOrbitalNumbers(ni, li, si, nf, lf, sf);
    M121 = ME::GetSingleParticleMatrixElement(
        false, std::abs(motherSpinParity) / 2., 1, 2, 1, ni, nf, li, lf, si, sf,
        R, nu);
    M101 = ME::GetSingleParticleMatrixElement(
        false, std::abs(motherSpinParity) / 2., 1, 0, 1, ni, nf, li, lf, si, sf,
        R, nu);
  } else if (boost::iequals(pot, "WS")) {
    M121 = ME::GetSingleParticleMatrixElement(
        false, std::abs(motherSpinParity) / 2., 1, 2, 1, spsi, spsf, R, nu);
    M101 = ME::GetSingleParticleMatrixElement(
        false, std::abs(motherSpinParity) / 2., 1, 0, 1, spsi, spsf, R, nu);
  } else if (boost::iequals(pot, "DWS")) {
    M121 = ME::CalculateDeformedSPMatrixElement(
        spsi, spsf, false, 1, 2, 1, spsi.dO, spsf.dO, spsi.dK, spsf.dK, R, nu);
    M101 = ME::CalculateDeformedSPMatrixElement(
        spsi, spsf, false, 1, 0, 1, spsi.dO, spsf.dO, spsi.dK, spsf.dK, R, nu);
  }
  cout << "M121: " << M121 << " M101: " << M101 << endl;
  fc1 = gA * M101;

  return M121 / M101;
}*/
