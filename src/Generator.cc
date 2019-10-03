#include "BSG/Generator.h"
#include "BSG/SpectralFunctions.h"
#include "BSG/ConfigParser.h"

#include "NHL/ParticleParser.h"
#include "NHL/Units/BetaDecayUnits.h"
#include "NHL/Math/Integration.h"
#include "PDS/Units/GlobalSystemOfUnits.h"

#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"

#include <iostream>
#include <stdio.h>
#include <cmath>

//#include "BSGConfig.h"

using std::cout;
using std::endl;

void ShowBSGInfo() {
  std::string author = "L. Hayen (leendert.hayen@kuleuven.be)";
  auto logger = spdlog::get("BSG_results_file");
  logger->info("{:*>60}", "");
  //logger->info("{:^60}", "BSG v" + std::string(BSG_VERSION));
  //logger->info("{:^60}", "Last update: " + std::string(BSG_LAST_UPDATE));
  logger->info("{:^60}", "Author: " + author);
  logger->info("{:*>60}\n", "");
}

namespace BSG {

  Generator::Generator(std::string _outputName) : outputName(_outputName) {
    InitializeLoggers();
    InitializeL0Constants();
    LoadExchangeParameters();
    debugFileLogger->debug("Leaving Generator constructor");
  }

  bool Generator::Initialize(std::string transitionFile, std::string configFile, int argc, const char** argv) {
    bool success = InitializeTransitionFromFile(transitionFile);
    InitializeOptionsFromConfigFile(configFile, argc, argv);
    return success;
  }

  bool Generator::InitializeTransitionFromFile(std::string filename) {
    TransitionOptions newTransitionOptions = ParseTransitionOptions(filename);
    if (TransitionSanityCheck(newTransitionOptions)) {
      transitionOptions = newTransitionOptions;
      InitializeBetaParams();
      return true;
    }
    return false;
  }

  void Generator::InitializeOptionsFromConfigFile(std::string filename, int argc, const char** argv) {
    configOptions = ParseConfigOptions(filename, argc, argv);
  }

  void Generator::InitializeBetaParams() {
    PDS::core::Nucleus* initNucleusDef = static_cast<PDS::core::Nucleus*>(transitionOptions.initNucleus.GetParticleDefinition());
    PDS::core::Nucleus* finalNucleusDef = static_cast<PDS::core::Nucleus*>(transitionOptions.finalNucleus.GetParticleDefinition());
    betaParams.Zi = initNucleusDef->GetZ();
    betaParams.Zf = finalNucleusDef->GetZ();
    betaParams.A = initNucleusDef->GetA();
    //sqrt(5/3) transforms <r^2>^{1/2} into the equivalent radius for a uniformly charged sphere
    betaParams.R = finalNucleusDef->GetRadius() / NHL::betaLength * std::sqrt(5./3.);
    if (transitionOptions.betaType == NHL::BETA_MINUS) {
      betaParams.W0 = (transitionOptions.QValue - transitionOptions.atomicEnergyDeficit
        + transitionOptions.initNucleus.GetExcitationEnergy()
        - transitionOptions.finalNucleus.GetExcitationEnergy()) / NHL::betaEnergy + 1;
    } else if (transitionOptions.betaType == NHL::BETA_PLUS) {
      betaParams.W0 = (transitionOptions.QValue - transitionOptions.atomicEnergyDeficit
        + transitionOptions.initNucleus.GetExcitationEnergy()
        - transitionOptions.finalNucleus.GetExcitationEnergy()) / NHL::betaEnergy - 1;
    }
    betaParams.exPars = exchangeCoefficients[initNucleusDef->GetZ()];

    for (int i = 0; i < bDim1; i++) {
      betaParams.aPos[i] = 0;
      betaParams.aNeg[i] = 0;
      for (int j = 0; j < bDim2; j++) {
        betaParams.aNeg[i] += bNeg[i][j] * std::pow(fine_structure_const * betaParams.Zf, j + 1);
        betaParams.aPos[i] += bPos[i][j] * std::pow(fine_structure_const * betaParams.Zf, j + 1);
      }
    }
  }

  // void Generator::InitializeShapeParameters() {
  //   debugFileLogger->debug("Entered InitializeShapeParameters");
  //   if (!BSGOptExists(Spectrum.ModGaussFit)) {
  //     hoFit = CD::FitHODist(Z, R * std::sqrt(3. / 5.));
  //   } else {
  //     hoFit = GetBSGOpt(double, Spectrum.ModGaussFit);
  //   }
  //   debugFileLogger->debug("hoFit: {}", hoFit);
  //
  //   ESShape = GetBSGOpt(std::string, Spectrum.ESShape);
  //   NSShape = GetBSGOpt(std::string, Spectrum.NSShape);
  //
  //   vOld.resize(3);
  //   vNew.resize(3);
  //
  //   if (ESShape == "Modified_Gaussian") {
  //     debugFileLogger->debug("Found Modified_Gaussian shape");
  //     vOld[0] = 3./2.;
  //     vOld[1] = -1./2.;
  //     vNew[0] = std::sqrt(5./2.)*4.*(1.+hoFit)*std::sqrt(2.+5.*hoFit)/std::sqrt(M_PI)*std::pow(2.+3.*hoFit, 3./2.);
  //     vNew[1] = -4./3./(3.*hoFit+2)/std::sqrt(M_PI)*std::pow(5.*(2.+5.*hoFit)/2./(2.+3.*hoFit), 3./2.);
  //     vNew[2] = (2.-7.*hoFit)/5./(3.*hoFit+2)/std::sqrt(M_PI)*std::pow(5.*(2.+5.*hoFit)/2./(2.+3.*hoFit), 5./3.);
  //   } else {
  //     if (BSGOptExists(Spectrum.vold) && BSGOptExists(Spectrum.vnew)) {
  //       debugFileLogger->debug("Found v and v'");
  //       vOld = GetBSGOpt(std::vector<double>, Spectrum.vold);
  //       vNew = GetBSGOpt(std::vector<double>, Spectrum.vnew);
  //     } else if (BSGOptExists(vold) || BSGOptExists(vnew)) {
  //       consoleLogger->error("ERROR: Both old and new potential expansions must be given.");
  //     }
  //   }
  //   debugFileLogger->debug("Leaving InitializeShapeParameters");
  // }
  //
  // void Generator::InitializeNSMInfo() {
  //   debugFileLogger->debug("Entering InitializeNSMInfo");
  //   nsm = new NS::NuclearStructureManager();
  //
  //   if (BSGOptExists(connect)) {
  //     int dKi, dKf;
  //     nsm->GetESPStates(spsi, spsf, dKi, dKf);
  //   }
  //
  //   GetMatrixElements();
  // }
  //
  // void Generator::GetMatrixElements() {
  //   debugFileLogger->info("Calculating matrix elements");
  //   double M101 = 1.0;
  //   if (!BSGOptExists(Spectrum.Lambda)) {
  //     M101 = nsm->CalculateReducedMatrixElement(false, 1, 0, 1);
  //     double M121 = nsm->CalculateReducedMatrixElement(false, 1, 2, 1);
  //     ratioM121 = M121 / M101;
  //   } else {
  //     ratioM121 = GetBSGOpt(double, Spectrum.Lambda);
  //   }
  //
  //   bAc = dAc = 0;
  //   if (!BSGOptExists(Spectrum.WeakMagnetism)) {
  //     debugFileLogger->info("Calculating Weak Magnetism");
  //     bAc = nsm->CalculateWeakMagnetism();
  //   } else {
  //     bAc = GetBSGOpt(double, Spectrum.WeakMagnetism);
  //   }
  //   if (!BSGOptExists(Spectrum.InducedTensor)) {
  //     debugFileLogger->info("Calculating Induced Tensor");
  //     dAc = nsm->CalculateInducedTensor();
  //   } else {
  //     dAc = GetBSGOpt(double, Spectrum.InducedTensor);
  //   }
  //
  //   if (std::isnan(bAc)) {
  //     bAc = 0.;
  //     consoleLogger->error("Calculated b/Ac was NaN. Setting to 0.");
  //   }
  //   if (std::isnan(dAc)) {
  //     dAc = 0.;
  //     consoleLogger->error("Calculated d/Ac was NaN. Setting to 0.");
  //   }
  //   if (std::isnan(ratioM121)) {
  //     ratioM121 = 0.;
  //     M101 = 1.;
  //     consoleLogger->error("Calculated M121/M101 was NaN. Setting ratio to 0 and M101 to 1.");
  //   }
  //
  //   if (M101 == 0.) {
  //     bAc = 0.;
  //     dAc = 0.;
  //     ratioM121 = 0.;
  //     M101 = 1.;
  //     consoleLogger->error("Calculated M101 is 0, resulting in infinities. Setting b/Ac, d/Ac and M121/M101 to 0 and M101 to 1.");
  //   }
  //
  //   debugFileLogger->info("Weak magnetism: {}", bAc);
  //   debugFileLogger->info("Induced tensor: {}", dAc);
  //   debugFileLogger->info("M121/M101: {}", ratioM121);
  //
  //   fc1 = gA * M101;
  //   fb = bAc * A * fc1;
  //   fd = dAc * A * fc1;
  // }

  std::vector<std::vector<double> >* Generator::CalculateSpectrum() {
    spectrum = new std::vector<std::vector<double> >();
    debugFileLogger->info("Calculating spectrum");
    double beginEn = configOptions.spectrumCalcOptions.begin * keV;
    double endEn = configOptions.spectrumCalcOptions.begin * keV;

    double beginW = beginEn / NHL::betaEnergy + 1.;
    double endW = endEn / NHL::betaEnergy + 1.;
    if (endEn == 0.0) {
      endW = betaParams.W0;
    }

    double stepW = configOptions.spectrumCalcOptions.stepSize / NHL::betaEnergy;
    if (configOptions.spectrumCalcOptions.steps) {
      stepW = (endW-beginW)/configOptions.spectrumCalcOptions.steps;
    }

    double currentW = beginW;
    while (currentW <= endW) {
      auto result = CalculateDecayRate(currentW);
      std::vector<double> entry = {currentW, std::get<0>(result), std::get<1>(result)};
      spectrum->push_back(entry);
      currentW += stepW;
    }
    PrepareOutputFile();
    return spectrum;
  }

  double Generator::CalculateLogFtValue(double partialHalflife) {
    debugFileLogger->debug("Calculating Ft value with partial halflife {}", partialHalflife);
    double f = NHL::Simpson(*spectrum);
    debugFileLogger->debug("f: {}", f);
    double ft = f*partialHalflife;
    double logFt = std::log10(ft);

    return logFt;
  }

  double Generator::CalculateMeanEnergy() {
    debugFileLogger->debug("Calculating mean energy");
    std::vector<std::vector<double> > weightedSpectrum;
    for (unsigned int i = 0; i < spectrum->size(); i++) {
      std::vector<double> entry = {(*spectrum)[i][0], (*spectrum)[i][0]* (*spectrum)[i][1]};
      weightedSpectrum.push_back(entry);
    }
    double weightedF = NHL::Simpson(weightedSpectrum);
    double f = NHL::Simpson(*spectrum);
    debugFileLogger->debug("Weighted f: {} Clean f: {}", weightedF, f);
    return weightedF/f;
  }

  std::tuple<double, double> Generator::CalculateDecayRate(double W) {
    //Setting constants for ease of use

    //Basic parameters
    double atomicEnergyDeficit = transitionOptions.atomicEnergyDeficit;
    double W0 = betaParams.W0;
    int Z = betaParams.Zf;
    int A = betaParams.A;
    double R = betaParams.R;
    int betaType = betaParams.betaType;
    NHL::BetaDecayType decayType = betaParams.decayType;
    double mixingRatio = betaParams.mixingRatio;
    std::array<double, 7> aNeg = betaParams.aNeg;
    std::array<double, 7> aPos = betaParams.aPos;
    std::array<double, 9> exPars = betaParams.exPars;

    //Coupling constants
    double gA = configOptions.couplingConstants.gA;
    double gP = configOptions.couplingConstants.gP;
    double gM = configOptions.couplingConstants.gM;

    //Matrix element ratios relevant for allowed beta decay
    double bAc = configOptions.allowedME.bAc;
    double dAc = configOptions.allowedME.dAc;
    double lambda = configOptions.allowedME.lambda;

    //Advanced options
    NHL::NuclearShapes NSShape = configOptions.advancedOptions.NSShape;
    NHL::NuclearShapes ESShape = configOptions.advancedOptions.ESShape;
    double modGaussFit = configOptions.advancedOptions.modGaussFit;
    std::vector<double> vOld = configOptions.advancedOptions.vold;
    std::vector<double> vNew = configOptions.advancedOptions.vnew;

    double result = 1;
    double neutrinoResult = 1;

    double Wv = W0 - W + 1;

    if (configOptions.correctionOptions.phaseSpace) {
      result *= SpectralFunctions::PhaseSpace(W, W0);
      neutrinoResult *= SpectralFunctions::PhaseSpace(Wv, W0);
    }
    if (configOptions.correctionOptions.FermiFunction) {
      result *= SpectralFunctions::FermiFunction(W, Z, R, betaType);
      neutrinoResult *= SpectralFunctions::FermiFunction(Wv, Z, R, betaType);
    }
    if (configOptions.correctionOptions.shapeFactor) {
      /*if (configOptions.advancedOptions.connectSPS) {
        result *= SpectralFunctions::CCorrection(W, W0, Z, A, R, betaType, decayType, gA,
                                  gP, fc1, fb, fd, ratioM121, configOptions.correctionOptions.isovector, NSShape, hoFit, spsi, spsf);
        neutrinoResult *=
            SpectralFunctions::CCorrection(Wv, W0, Z, A, R, betaType, decayType, gA, gP,
                            fc1, fb, fd, ratioM121, configOptions.correctionOptions.isovector, NSShape, hoFit, spsi, spsf);
      } else {*/
        result *= SpectralFunctions::CCorrection(W, W0, Z, A, R, betaType, decayType, gA,
                                  gP, bAc, dAc, lambda, configOptions.correctionOptions.isovector, NSShape, modGaussFit);
        neutrinoResult *=
            SpectralFunctions::CCorrection(Wv, W0, Z, A, R, betaType, decayType, gA, gP,
                            bAc, dAc, lambda, configOptions.correctionOptions.isovector, NSShape, modGaussFit);
      //}
    }
    if (configOptions.correctionOptions.relativistic) {
      result *= SpectralFunctions::RelativisticCorrection(W, W0, Z, R, betaType, decayType);
      neutrinoResult *=
          SpectralFunctions::RelativisticCorrection(Wv, W0, Z, R, betaType, decayType);
    }
    if (configOptions.correctionOptions.ESDeformation) {
      /*result *=
          SpectralFunctions::DeformationCorrection(W, W0, Z, R, daughterBeta2, betaType, aPos, aNeg);
      neutrinoResult *=
          SpectralFunctions::DeformationCorrection(Wv, W0, Z, R, daughterBeta2, betaType, aPos, aNeg);*/
    }
    if (configOptions.correctionOptions.ESFiniteSize) {
      result *= SpectralFunctions::L0Correction(W, Z, R, betaType, aPos, aNeg);
      neutrinoResult *= SpectralFunctions::L0Correction(Wv, Z, R, betaType, aPos, aNeg);
    }
    if (configOptions.correctionOptions.ESFermi) {
      result *= SpectralFunctions::UCorrection(W, Z, R, betaType, ESShape, vOld, vNew);
      neutrinoResult *= SpectralFunctions::UCorrection(Wv, Z, R, betaType, ESShape, vOld, vNew);
    }
    if (configOptions.correctionOptions.CoulombRecoil) {
      result *= SpectralFunctions::QCorrection(W, W0, Z, A, betaType, decayType, mixingRatio);
      neutrinoResult *=
          SpectralFunctions::QCorrection(Wv, W0, Z, A, betaType, decayType, mixingRatio);
    }
    if (configOptions.correctionOptions.radiative) {
      result *= SpectralFunctions::RadiativeCorrection(W, W0, Z, R, betaType, gA, gM);
      neutrinoResult *= SpectralFunctions::NeutrinoRadiativeCorrection(Wv);
    }
    if (configOptions.correctionOptions.kinRecoil) {
      result *= SpectralFunctions::RecoilCorrection(W, W0, A, decayType, mixingRatio);
      neutrinoResult *= SpectralFunctions::RecoilCorrection(Wv, W0, A, decayType, mixingRatio);
    }
    if (configOptions.correctionOptions.atomicScreen) {
      result *= SpectralFunctions::AtomicScreeningCorrection(W, Z, betaType);
      neutrinoResult *= SpectralFunctions::AtomicScreeningCorrection(Wv, Z, betaType);
    }
    if (configOptions.correctionOptions.atomicExchange) {
      if (betaType == NHL::BETA_MINUS) {
        result *= SpectralFunctions::AtomicExchangeCorrection(W, exPars);
        neutrinoResult *= SpectralFunctions::AtomicExchangeCorrection(Wv, exPars);
      }
    }
    if (configOptions.correctionOptions.atomicMismatch) {
      if (atomicEnergyDeficit == 0.) {
        result *= SpectralFunctions::AtomicMismatchCorrection(W, W0, Z, A, betaType);
        neutrinoResult *= SpectralFunctions::AtomicMismatchCorrection(Wv, W0, Z, A, betaType);
      }
    }
    result = std::max(0., result);
    neutrinoResult = std::max(0., neutrinoResult);
    //TODO Units
    rawSpectrumLogger->info("{:<10f}\t{:<10f}\t{:<10f}\t{:<10f}", W, (W-1.) * NHL::betaEnergy / keV, result, neutrinoResult);
    return std::make_tuple(result, neutrinoResult);
  }

  void Generator::PrepareOutputFile() {
    ShowBSGInfo();

/*    auto l = spdlog::get("BSG_results_file");
    l->info("Spectrum input overview\n{:=>30}", "");
    //l->info("Using information from {}\n\n", GetBSGOpt(std::string, input));
    l->info("Transition from {}{} [{}/2] ({} keV) to {}{} [{}/2] ({} keV)", A, utilities::atoms[int(Z-1-betaType)], motherSpinParity, motherExcitationEn, A, utilities::atoms[int(Z-1)], daughterSpinParity, daughterExcitationEn);
    l->info("Q Value: {} keV\tEffective endpoint energy: {}", transitionOptions.QValue, (W0-1.)*ELECTRON_MASS_KEV);
    l->info("Process: {}\tType: {}", GetBSGOpt(std::string, Transition.Process), GetBSGOpt(std::string, Transition.Type));
    if (mixingRatio != 0) l->info("Mixing ratio: {}", mixingRatio);

    // double BGT = fc1*fc1/(std::abs(motherSpinParity)+1)
    // double kappa = 6147.; // The combination of constants for the ft value in s
    // double ftTheory = std::log10(kappa/BGT);
    // l->info("");
    if (BSGOptExists(Transition.PartialHalflife)) {
      l->info("Partial halflife: {} s", GetBSGOpt(double, Transition.PartialHalflife));
      l->info("Calculated log ft value: {}", CalculateLogFtValue(GetBSGOpt(double, Transition.PartialHalflife)));
    } else {
      l->info("Partial halflife: not given");
      l->info("Calculated log f value: {}", CalculateLogFtValue(1.0));
    }
    if (BSGOptExists(Transition.LogFt)) {
      l->info("External Log ft: {:.3f}", GetBSGOpt(double, Transition.LogFt));
      if (BSGOptExists(Transition.PartialHalflife)) {
        l->info("Ratio of calculated/external ft value: {}", std::pow(10.,
          CalculateLogFtValue(GetBSGOpt(double, Transition.PartialHalflife))
           - GetBSGOpt(double, Transition.LogFt)));
      }
    }
    l->info("Mean energy: {} keV", (CalculateMeanEnergy()-1.)*ELECTRON_MASS_KEV);
    l->info("\nMatrix Element Summary\n{:->30}", "");
    if (BSGOptExists(Spectrum.WeakMagnetism)) l->info("{:35}: {} ({})", "b/Ac (weak magnetism)", bAc, "given");
    else l->info("{:35}: {}", "b/Ac (weak magnetism)", bAc);
    if (BSGOptExists(Spectrum.Inducedtensor)) l->info("{:35}: {} ({})", "d/Ac (induced tensor)", dAc, "given");
    else l->info("{:35}: {}", "d/Ac (induced tensor)", dAc);
    if (BSGOptExists(Spectrum.Lambda)) l->info("{:35}: {} ({})", "AM121/AM101", ratioM121, "given");
    else l->info("{:35}: {}", "AM121/AM101", ratioM121);

    l->info("Full breakdown written in {}.nme", outputName);

    l->info("\nSpectral corrections\n{:->30}", "");
    l->info("{:25}: {}", "Phase space", GetBSGOpt(bool, Spectrum.Phasespace));
    l->info("{:25}: {}", "Fermi function", GetBSGOpt(bool, Spectrum.Fermi));
    l->info("{:25}: {}", "L0 correction", GetBSGOpt(bool, Spectrum.ESFiniteSize));
    l->info("{:25}: {}", "C correction", GetBSGOpt(bool, Spectrum.C));
    l->info("    NS Shape: {}", GetBSGOpt(std::string, Spectrum.NSShape));
    l->info("{:25}: {}", "Isovector correction", GetBSGOpt(bool, Spectrum.Isovector));
    l->info("    Connected: {}", GetBSGOpt(bool, Spectrum.Connect));
    l->info("{:25}: {}", "Relativistic terms", GetBSGOpt(bool, Spectrum.Relativistic));
    l->info("{:25}: {}", "Deformation", GetBSGOpt(bool, Spectrum.ESDeformation));
    l->info("{:25}: {}", "U correction", GetBSGOpt(bool, Spectrum.U));
    l->info("    ES Shape: {}", GetBSGOpt(std::string, Spectrum.ESShape));
    if (BSGOptExists(Spectrum.vold) && BSGOptExists(Spectrum.vnew)) {
      l->info("    v : {}, {}, {}", vOld[0], vOld[1], vOld[2]);
      l->info("    v': {}, {}, {}", vNew[0], vNew[1], vNew[2]);
    } else {
      l->info("    v : not given");
      l->info("    v': not given");
    }
    l->info("{:25}: {}", "Q correction", GetBSGOpt(bool, Spectrum.CoulombRecoil));
    l->info("{:25}: {}", "Radiative correction", GetBSGOpt(bool, Spectrum.Radiative));
    l->info("{:25}: {}", "Nuclear recoil", GetBSGOpt(bool, Spectrum.Recoil));
    l->info("{:25}: {}", "Atomic screening", GetBSGOpt(bool, Spectrum.Screening));
    l->info("{:25}: {}", "Atomic exchange", GetBSGOpt(bool, Spectrum.Exchange));
    l->info("{:25}: {}", "Atomic mismatch", GetBSGOpt(bool, Spectrum.AtomicMismatch));
    l->info("{:25}: {}", "Export neutrino", GetBSGOpt(bool, Spectrum.Neutrino));

    l->info("\n\nSpectrum calculated from {} keV to {} keV with step size {} keV\n",
    GetBSGOpt(double, Spectrum.Begin),
    GetBSGOpt(double, Spectrum.End) > 0 ? GetBSGOpt(double, Spectrum.End) : (W0-1.)*ELECTRON_MASS_KEV, GetBSGOpt(double, Spectrum.StepSize));

    if (GetBSGOpt(bool, Spectrum.Neutrino))  l->info("{:10}\t{:10}\t{:10}\t{:10}", "W [m_ec2]", "E [keV]", "dN_e/dW", "dN_v/dW");
    else l->info("{:10}\t{:10}\t{:10}", "W [m_ec2]", "E [keV]", "dN_e/dW");

    for (int i = 0; i < spectrum->size(); i++) {
      if (GetBSGOpt(bool, Spectrum.Neutrino)) {
        l->info("{:<10f}\t{:<10f}\t{:<10f}\t{:<10f}", (*spectrum)[i][0], ((*spectrum)[i][0]-1.)*ELECTRON_MASS_KEV, (*spectrum)[i][1], (*spectrum)[i][2]);
      } else {
        l->info("{:<10f}\t{:<10f}\t{:<10f}", (*spectrum)[i][0], ((*spectrum)[i][0]-1.)*ELECTRON_MASS_KEV, (*spectrum)[i][1]);
      }
    }*/
  }

  //Initialization/loading

  void Generator::InitializeLoggers() {
    /**
     * Remove result & log files if they already exist
     */
    if (std::ifstream(outputName + ".log")) std::remove((outputName + ".log").c_str());
    if (std::ifstream(outputName + ".raw")) std::remove((outputName + ".raw").c_str());
    if (std::ifstream(outputName + ".txt")) std::remove((outputName + ".txt").c_str());

    debugFileLogger = spdlog::get("debug_file");
    if (!debugFileLogger) {
      debugFileLogger = spdlog::basic_logger_mt("debug_file", outputName + ".log");
      debugFileLogger->set_level(spdlog::level::debug);
    }
    debugFileLogger->debug("Debugging logger created");
    consoleLogger = spdlog::get("console");
    if (!consoleLogger) {
      consoleLogger = spdlog::stdout_color_mt("console");
      consoleLogger->set_pattern("%v");
      consoleLogger->set_level(spdlog::level::warn);
    }
    debugFileLogger->debug("Console logger created");
    rawSpectrumLogger = spdlog::get("BSG_raw");
    if (!rawSpectrumLogger) {
      rawSpectrumLogger = spdlog::basic_logger_mt("BSG_raw", outputName + ".raw");
      rawSpectrumLogger->set_pattern("%v");
      rawSpectrumLogger->set_level(spdlog::level::info);
    }
    debugFileLogger->debug("Raw spectrum logger created");
    resultsFileLogger = spdlog::get("BSG_results_file");
    if (!resultsFileLogger) {
      resultsFileLogger = spdlog::basic_logger_mt("BSG_results_file", outputName + ".txt");
      resultsFileLogger->set_pattern("%v");
      resultsFileLogger->set_level(spdlog::level::info);
    }
    debugFileLogger->debug("Results file logger created");
  }

  void Generator::LoadExchangeParameters() {
    debugFileLogger->debug("Entered LoadExchangeParameters");
    //TODO
    //std::string exParamFile = GetBSGOpt(std::string, exchangedata);
    std::string exParamFile = "";
    std::ifstream paramStream(exParamFile.c_str());
    std::string line;

    if (paramStream.is_open()) {
      while (getline(paramStream, line)) {
        std::array<double, 9> exPars;
        int z;

        std::istringstream iss(line);
        iss >> z >> exPars[0] >> exPars[1] >> exPars[2] >> exPars[3] >> exPars[4]
        >> exPars[5] >> exPars[6] >> exPars[7] >> exPars[8];

        exchangeCoefficients[z] = exPars;
      }
    } else {
      consoleLogger->error("ERROR: Can't find Exchange parameters file at {}.", exParamFile);
    }
    debugFileLogger->debug("Leaving LoadExchangeParameters");
  }

  void Generator::InitializeL0Constants() {
    debugFileLogger->debug("Entering InitializeL0Constants");
    bNeg[0][0] = 0.115;
    bNeg[0][1] = -1.8123;
    bNeg[0][2] = 8.2498;
    bNeg[0][3] = -11.223;
    bNeg[0][4] = -14.854;
    bNeg[0][5] = 32.086;
    bNeg[1][0] = -0.00062;
    bNeg[1][1] = 0.007165;
    bNeg[1][2] = 0.01841;
    bNeg[1][3] = -0.53736;
    bNeg[1][4] = 1.2691;
    bNeg[1][5] = -1.5467;
    bNeg[2][0] = 0.02482;
    bNeg[2][1] = -0.5975;
    bNeg[2][2] = 4.84199;
    bNeg[2][3] = -15.3374;
    bNeg[2][4] = 23.9774;
    bNeg[2][5] = -12.6534;
    bNeg[3][0] = -0.14038;
    bNeg[3][1] = 3.64953;
    bNeg[3][2] = -38.8143;
    bNeg[3][3] = 172.1368;
    bNeg[3][4] = -346.708;
    bNeg[3][5] = 288.7873;
    bNeg[4][0] = 0.008152;
    bNeg[4][1] = -1.15664;
    bNeg[4][2] = 49.9663;
    bNeg[4][3] = -273.711;
    bNeg[4][4] = 657.6292;
    bNeg[4][5] = -603.7033;
    bNeg[5][0] = 1.2145;
    bNeg[5][1] = -23.9931;
    bNeg[5][2] = 149.9718;
    bNeg[5][3] = -471.2985;
    bNeg[5][4] = 662.1909;
    bNeg[5][5] = -305.6804;
    bNeg[6][0] = -1.5632;
    bNeg[6][1] = 33.4192;
    bNeg[6][2] = -255.1333;
    bNeg[6][3] = 938.5297;
    bNeg[6][4] = -1641.2845;
    bNeg[6][5] = 1095.358;

    bPos[0][0] = 0.0701;
    bPos[0][1] = -2.572;
    bPos[0][2] = 27.5971;
    bPos[0][3] = -128.658;
    bPos[0][4] = 272.264;
    bPos[0][5] = -214.925;
    bPos[1][0] = -0.002308;
    bPos[1][1] = 0.066463;
    bPos[1][2] = -0.6407;
    bPos[1][3] = 2.63606;
    bPos[1][4] = -5.6317;
    bPos[1][5] = 4.0011;
    bPos[2][0] = 0.07936;
    bPos[2][1] = -2.09284;
    bPos[2][2] = 18.45462;
    bPos[2][3] = -80.9375;
    bPos[2][4] = 160.8384;
    bPos[2][5] = -124.8927;
    bPos[3][0] = -0.93832;
    bPos[3][1] = 22.02513;
    bPos[3][2] = -197.00221;
    bPos[3][3] = 807.1878;
    bPos[3][4] = -1566.6077;
    bPos[3][5] = 1156.3287;
    bPos[4][0] = 4.276181;
    bPos[4][1] = -96.82411;
    bPos[4][2] = 835.26505;
    bPos[4][3] = -3355.8441;
    bPos[4][4] = 6411.3255;
    bPos[4][5] = -4681.573;
    bPos[5][0] = -8.2135;
    bPos[5][1] = 179.0862;
    bPos[5][2] = -1492.1295;
    bPos[5][3] = 5872.5362;
    bPos[5][4] = -11038.7299;
    bPos[5][5] = 7963.4701;
    bPos[6][0] = 5.4583;
    bPos[6][1] = -115.8922;
    bPos[6][2] = 940.8305;
    bPos[6][3] = -3633.9181;
    bPos[6][4] = 6727.6296;
    bPos[6][5] = -4795.0481;
    debugFileLogger->debug("Leaving InitializeL0Constants");
  }
}
