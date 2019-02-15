// *****************************************************
// Processor for isolated lepton tagging
//                        ----originally developped by C.Duerig and J.Tian
//                        ----for Higgs self-coupling analysis
// *****************************************************
#include "myIsolatedLeptonTaggingProcessor.h"

#include <EVENT/LCCollection.h>
#include <EVENT/MCParticle.h>
#include <IMPL/LCCollectionVec.h>
#include <EVENT/ReconstructedParticle.h>
#include <EVENT/Cluster.h>
#include <UTIL/LCTypedVector.h>
#include <EVENT/Track.h>
#include <marlin/Exceptions.h>
#include <EVENT/Vertex.h>

// ----- include for verbosity dependend logging ---------
#include "marlin/VerbosityLevels.h"

//root
#include "TNtupleD.h"
#include "TVector3.h"
#include "TMath.h"
#include "TLorentzVector.h"
#include "TMVA/Reader.h"
//local
#include "Utilities.h"

using namespace lcio ;
using namespace marlin ;
using namespace std;

using namespace TMVA;
using namespace mylib;

myIsolatedLeptonTaggingProcessor amyIsolatedLeptonTaggingProcessor ;


myIsolatedLeptonTaggingProcessor::myIsolatedLeptonTaggingProcessor() : Processor("myIsolatedLeptonTaggingProcessor") {
  
  // modify processor description
  _description = "myIsolatedLeptonTaggingProcessor does whatever it does ..." ;
  

  // register steering parameters: name, description, class-variable, default value

  registerInputCollection( LCIO::MCPARTICLE,
			   "InputMCParticlesCollection" , 
			   "Name of the MCParticle collection"  ,
			   _colMCP ,
			   std::string("MCParticlesSkimmed") ) ;
  
  registerInputCollection( LCIO::RECONSTRUCTEDPARTICLE,
			   "InputPandoraPFOsCollection" , 
			   "Name of the PandoraPFOs collection"  ,
			   _colPFOs ,
			   std::string("PandoraPFOs") ) ;

  registerInputCollection( LCIO::VERTEX,
			   "InputPrimaryVertexCollection" ,
			   "Name of the Primary Vertex collection"  ,
			   _colPVtx ,
			   std::string("PrimaryVertex") ) ;

  registerOutputCollection( LCIO::RECONSTRUCTEDPARTICLE, 
			    "OutputPFOsWithoutIsoLepCollection",
			    "Name of the new PFOs collection without isolated lepton",
			    _colNewPFOs,
			    std::string("PandoraPFOsWithoutIsoLep") );
  
  registerOutputCollection( LCIO::RECONSTRUCTEDPARTICLE, 
			    "OutputIsoLeptonsCollection",
			    "Name of collection with the selected isolated lepton",
			    _colLeptons,
			    std::string("ISOLeptons") );
  
  registerProcessorParameter("DirOfISOElectronWeights",
			     "Directory of Weights for the Isolated Electron MVA Classification"  ,
			     _isolated_electron_weights ,
			     std::string("isolated_electron_weights") ) ;
  
  registerProcessorParameter("DirOfISOMuonWeights",
			     "Directory of Weights for the Isolated Muon MVA Classification"  ,
			     _isolated_muon_weights ,
			     std::string("isolated_muon_weights") ) ;
  
  registerProcessorParameter("CutOnTheISOElectronMVA",
			     "Cut on the mva output of isolated electron selection"  ,
			     _mvaCutForElectron ,
			     float(0.5) ) ;
  
  registerProcessorParameter("CutOnTheISOMuonMVA",
			     "Cut on the mva output of isolated muon selection"  ,
			     _mvaCutForMuon ,
			     float(0.7) ) ;
  
  registerProcessorParameter("IsSelectingOneIsoLep",
			     "flag to select one most like isolated lepton"  ,
			     _is_one_isolep ,
			     bool(true) ) ;
  
  registerProcessorParameter("MinEOverPForElectron",
			     "minimum ratio of energy in calorimeters over momentum for electron"  ,
			     _minEOverPForElectron ,
			     float(0.5) ) ;
  
  registerProcessorParameter("MaxEOverPForElectron",
			     "Maximum ratio of energy in calorimeters over momentum for electron"  ,
			     _maxEOverPForElectron ,
			     float(1.3) ) ;
  
  registerProcessorParameter("MinEecalOverTotEForElectron",
			     "minimum ratio of energy in ecal over energy in ecal+hcal"  ,
			     _minEecalOverTotEForElectron ,
			     float(0.9) ) ;
  
  registerProcessorParameter("MaxD0SigForElectron",
			     "Maximum d0 significance for electron"  ,
			     _maxD0SigForElectron ,
			     float(50.) ) ;
  
  registerProcessorParameter("MaxZ0SigForElectron",
			     "Maximum Z0 significance for electron"  ,
			     _maxZ0SigForElectron ,
			     float(5.) ) ;
  
  registerProcessorParameter("MinPForElectron",
			     "Minimum momentum for electron"  ,
			     _minPForElectron ,
			     float(5.) ) ;
  
  registerProcessorParameter("MaxEOverPForMuon",
			     "Maximum ratio of energy in calorimeters over momentum for muon"  ,
			     _maxEOverPForMuon ,
			     float(0.3) ) ;
  
  registerProcessorParameter("MinEyokeForMuon",
			     "Minimum energy in yoke for electron"  ,
			     _minEyokeForMuon ,
			     float(1.2) ) ;
  
  registerProcessorParameter("MaxD0SigForMuon",
			     "Maximum D0 significance for muon"  ,
			     _maxD0SigForMuon ,
			     float(5.) ) ;
  
  registerProcessorParameter("MaxZ0SigForMuon",
			     "Maximum Z0 significance for muon"  ,
			     _maxZ0SigForMuon ,
			     float(5.) ) ;
  
  registerProcessorParameter("MinPForMuon",
			     "Minimum momentum for muon"  ,
			     _minPForMuon ,
			     float(5.) ) ;
  
  registerProcessorParameter("CosConeSmall",
			     "cosine of the smaller cone"  ,
			     _cosConeSmall ,
			     float(0.98) ) ;
  
  registerProcessorParameter("CosConeLarge",
			     "cosine of the larger cone"  ,
			     _cosConeLarge ,
			     float(0.95) ) ;

  registerProcessorParameter("UseYokeForMuonID",
			     "use yoke for muon ID"  ,
			     _use_yoke_for_muon ,
			     bool(false) ) ;
}

void myIsolatedLeptonTaggingProcessor::init() { 

  streamlog_out(DEBUG) << "IsolatedLeptonTagging   init called  " 
		       << std::endl ;
  
  
  // usually a good idea to
  printParameters() ;


  for (Int_t i=0;i<2;i++) {
    TMVA::Reader *reader = new TMVA::Reader( "Color:Silent" );    
    // add input variables
    if (i == 0) {  // electron
      reader->AddVariable( "coneec",          &_coneec ); // neutral energy in the smaller cone
      reader->AddVariable( "coneen",          &_coneen ); // charged energy in the smaller cone
      reader->AddVariable( "momentum",        &_momentum ); // momentum
      reader->AddVariable( "coslarcon",       &_coslarcon ); // angle between lep and large cone momentum
      reader->AddVariable( "energyratio",     &_energyratio ); // energy ration of lep and large cone
      reader->AddVariable( "ratioecal",       &_ratioecal ); // ratio of energy in ecal over energy in ecal+hcal
      reader->AddVariable( "ratiototcal",     &_ratiototcal ); // ratio of energy in ecal+hcal over momentum
      reader->AddVariable( "nsigd0",          &_nsigd0 ); // significance of d0
      reader->AddVariable( "nsigz0",          &_nsigz0 ); // significance of z0
    }
    else { // muon
      reader->AddVariable( "coneec",          &_coneec );
      reader->AddVariable( "coneen",          &_coneen );
      reader->AddVariable( "momentum",        &_momentum );
      reader->AddVariable( "coslarcon",       &_coslarcon );
      reader->AddVariable( "energyratio",     &_energyratio );
      if (_use_yoke_for_muon)
	reader->AddVariable( "yokeenergy",      &_yokeenergy ); // energy in yoke
      reader->AddVariable( "nsigd0",          &_nsigd0 );
      reader->AddVariable( "nsigz0",          &_nsigz0 );
      reader->AddVariable( "totalcalenergy",  &_totalcalenergy );
    }
    
    // book the reader (method, weights)
    TString dir    = _isolated_electron_weights;
    if (i == 1) dir = _isolated_muon_weights;
    TString prefix = "TMVAClassification";
    TString methodName = "MLP method";
    TString weightfile = dir + "/" + prefix + "_" + "MLP.weights.xml";
    reader->BookMVA( methodName, weightfile ); 
    _readers.push_back(reader);
  }
  
}

void myIsolatedLeptonTaggingProcessor::processRunHeader( LCRunHeader* run) { 
} 

void myIsolatedLeptonTaggingProcessor::processEvent( LCEvent * evt ) { 
  
  
  //streamlog_out(DEBUG) << "Hello, Isolated Lepton Tagging!" << endl;
  //if ( (evt->getEventNumber()%10) == 0 )
  //cerr << "Hello, myIsolated Lepton Tagging!" << endl;
  
  // get hGen

  static TNtupleD *hGen = 0;
  if (!hGen) {
    stringstream tupstr_gen;
    tupstr_gen << "nhbb:nhww:nhgg:nhtt:nhcc:nhzz:nhaa:nhmm" << ":"
	       << "ej1mc:ej2mc:cosj1mc:cosj2mc:ptzmc:coszmc:mzmc:acop12mc:mrecoilmc" << ":"
	       << "eisr1mc:eisr2mc"
	       << ends;
    hGen = new TNtupleD("hGen","",tupstr_gen.str().data());
  }

  // -- read out the MCParticles information
  LCCollection *colMC = evt->getCollection(_colMCP);
  // get the truth information
  Int_t nMCP = colMC->getNumberOfElements();
  TLorentzVector lortzJ1MC,lortzJ2MC,lortzZMC,lortzHMC,lortzISRMC;
  TLorentzVector lortzISR1MC, lortzISR2MC;
  for (Int_t i=0;i<nMCP;i++) {
    MCParticle *mcPart = dynamic_cast<MCParticle*>(colMC->getElementAt(i));
    Int_t pdg = mcPart->getPDG();
    Int_t nparents = mcPart->getParents().size();
    Int_t motherpdg = 0;
    if (nparents > 0) {
      MCParticle *mother = mcPart->getParents()[0];
      motherpdg = mother->getPDG();
    }
    Double_t energy = mcPart->getEnergy();
    TVector3 pv = TVector3(mcPart->getMomentum());
    Int_t ndaughters = mcPart->getDaughters().size();
    Int_t daughterpdg = 0;
    if (ndaughters > 0) {
      MCParticle *daughter = mcPart->getDaughters()[0];
      daughterpdg = daughter->getPDG();
    }
    TLorentzVector lortz = TLorentzVector(pv,energy);
    Int_t ioverlay = mcPart->isOverlay()? 1 : 0;
    if ((pdg > 0 && pdg < 10) && motherpdg == 0 && ioverlay == 0) {
      lortzJ1MC = lortz;
      lortzZMC += lortz;
    }
    if ((pdg > -10 && pdg < 0) && motherpdg == 0 && ioverlay == 0) {
      lortzJ2MC = lortz;
      lortzZMC += lortz;
    }
    if (pdg == 25 && motherpdg == 0 && ioverlay == 0) {
      lortzHMC = lortz;
    }
    if (i == 0) {
      lortzISR1MC = lortz;
    }
    if (i == 1) {
      lortzISR2MC = lortz;
    }
  }
  // get Higgs decay modes
  std::vector<Int_t> nHDecay;
  nHDecay = getHiggsDecayModes(colMC);
  Double_t nHbb = nHDecay[0]; // H---> b b
  Double_t nHWW = nHDecay[1]; // H---> W W
  Double_t nHgg = nHDecay[2]; // H---> g g
  Double_t nHtt = nHDecay[3]; // H---> tau tau
  Double_t nHcc = nHDecay[4]; // H---> c c
  Double_t nHZZ = nHDecay[5]; // H---> Z Z
  Double_t nHaa = nHDecay[6]; // H---> gam gam
  Double_t nHmm = nHDecay[7]; // H---> mu mu

  const Double_t fEcm = 500.;
  TLorentzVector lortzEcm = getLorentzEcm(fEcm);
  
  Double_t data_gen[50];
  data_gen[ 0] = nHbb;
  data_gen[ 1] = nHWW;
  data_gen[ 2] = nHgg;
  data_gen[ 3] = nHtt;
  data_gen[ 4] = nHcc;
  data_gen[ 5] = nHZZ;
  data_gen[ 6] = nHaa;
  data_gen[ 7] = nHmm;
  data_gen[ 8] = lortzJ1MC.E();
  data_gen[ 9] = lortzJ2MC.E();
  data_gen[10] = lortzJ1MC.CosTheta();
  data_gen[11] = lortzJ2MC.CosTheta();
  data_gen[12] = lortzZMC.Pt();
  data_gen[13] = lortzZMC.CosTheta();
  data_gen[14] = lortzZMC.M();
  data_gen[15] = getAcoPlanarity(lortzJ1MC,lortzJ2MC);
  data_gen[16] = getRecoilMass(lortzEcm,lortzZMC);
  data_gen[17] = lortzISR1MC.E();
  data_gen[18] = lortzISR2MC.E();
  hGen->Fill(data_gen);

  // -- get Primary Vertex collection --
  // primary vertex from VertexFinder (LCFIPlus)
  LCCollection *colPVtx = evt->getCollection(_colPVtx);
  Vertex *pvtx = dynamic_cast<Vertex*>(colPVtx->getElementAt(0));
  Double_t z_pvtx = pvtx->getPosition()[2];
  
  // -- get PFO collection --
  LCCollection *colPFO = evt->getCollection(_colPFOs);
  if (!colPFO) {
    std::cerr << "No PFO Collection Found!" << std::endl;
    throw marlin::SkipEventException(this);
  }
  Int_t nPFOs = colPFO->getNumberOfElements();
  std::vector<lcio::ReconstructedParticle*> newPFOs;
  std::vector<lcio::ReconstructedParticle*> isoLeptons;
  FloatVec isoLepTagging;
  IntVec   isoLepType;

  // loop all the PFOs
  float _mva_lep_max = -1.;
  int _lep_type = 0;
  for (Int_t i=0;i<nPFOs;i++) {
    ReconstructedParticle *recPart = dynamic_cast<ReconstructedParticle*>(colPFO->getElementAt(i));
    Double_t energy = recPart->getEnergy();
    Double_t charge = recPart->getCharge();
    // get track impact parameters
    TrackVec tckvec = recPart->getTracks();
    Int_t ntracks = tckvec.size();
    Double_t d0=0.,z0=0.,deltad0=0.,deltaz0=0.,nsigd0=0.,nsigz0=0.;
    if (ntracks > 0) {
      d0 = tckvec[0]->getD0();
      z0 = tckvec[0]->getZ0();
      z0 -= z_pvtx; // new version 201808
      deltad0 = TMath::Sqrt(tckvec[0]->getCovMatrix()[0]);
      deltaz0 = TMath::Sqrt(tckvec[0]->getCovMatrix()[9]);
      nsigd0 = d0/deltad0;
      nsigz0 = z0/deltaz0;
    }
    // here in principle can add any precut on each PFO
    newPFOs.push_back(recPart);
    // get energies deposited in each subdetector
    Double_t ecalEnergy = 0;
    Double_t hcalEnergy = 0;
    Double_t yokeEnergy = 0;
    Double_t totalCalEnergy = 0;
    std::vector<lcio::Cluster*> clusters = recPart->getClusters();
    for (std::vector<lcio::Cluster*>::const_iterator iCluster=clusters.begin();iCluster!=clusters.end();++iCluster) {
      ecalEnergy += (*iCluster)->getSubdetectorEnergies()[0];
      hcalEnergy += (*iCluster)->getSubdetectorEnergies()[1];
      yokeEnergy += (*iCluster)->getSubdetectorEnergies()[2];
      ecalEnergy += (*iCluster)->getSubdetectorEnergies()[3];
      hcalEnergy += (*iCluster)->getSubdetectorEnergies()[4];
    }
    totalCalEnergy = ecalEnergy + hcalEnergy;
    TVector3 momentum = TVector3(recPart->getMomentum());
    Double_t momentumMagnitude = momentum.Mag();
    // get cone information
    // double cone based isolation algorithm
    Bool_t woFSR = kTRUE; // by default bremsstrahlung and FSR are not included in cone energy
    Double_t coneEnergy0[3] = {0.,0.,0.};  // {total, neutral, charged} cone energy
    Double_t pFSR[4] = {0.,0.,0.,0.};  // 4-momentum of BS/FSR if any found, not being used by MVA
    Double_t pLargeCone[4]  = {0.,0.,0.,0.}; // 4-momentum of all particles inside the larger cone
    Int_t nConePhoton = 0;  // number of BS/FSR photons found, not being used by MVA
    getConeEnergy(recPart,colPFO,_cosConeSmall,woFSR,coneEnergy0,pFSR,_cosConeLarge,pLargeCone,nConePhoton);
    Double_t coneEN     = coneEnergy0[1];
    Double_t coneEC     = coneEnergy0[2];
    TLorentzVector lortzLargeCone = TLorentzVector(pLargeCone[0],pLargeCone[1],pLargeCone[2],pLargeCone[3]);
    TVector3 momentumLargeCone = lortzLargeCone.Vect();
    Double_t cosThetaWithLargeCone = 1.;
    if (momentumLargeCone.Mag() > 0.0000001) {
      cosThetaWithLargeCone = momentum.Dot(momentumLargeCone)/momentumMagnitude/momentumLargeCone.Mag();
    }
    Double_t energyRatioWithLargeCone = energy/(energy+lortzLargeCone.E());
    Double_t ratioECal = 0., ratioTotalCal = 0.;
    if (ecalEnergy > 0.) ratioECal = ecalEnergy/totalCalEnergy;
    ratioTotalCal = totalCalEnergy/momentumMagnitude;
    // all the input variables to MVA are ready
    // evaluate the neural-net output of isolated-lepton classfication
    Double_t mva_electron = -1.,mva_muon = -1.;
    _coneec      = coneEC;
    _coneen      = coneEN;
    _momentum    = momentumMagnitude;
    _coslarcon   = cosThetaWithLargeCone;
    _energyratio = energyRatioWithLargeCone;
    _ratioecal   = ratioECal;
    _ratiototcal = ratioTotalCal;
    _nsigd0      = nsigd0;
    _nsigz0      = nsigz0;
    _yokeenergy  = yokeEnergy;
    _totalcalenergy = totalCalEnergy;
    Double_t fEpsilon = 1.E-10;
    if (charge != 0 && 
	totalCalEnergy/momentumMagnitude > _minEOverPForElectron &&
	totalCalEnergy/momentumMagnitude < _maxEOverPForElectron &&
	ecalEnergy/(totalCalEnergy + fEpsilon) > _minEecalOverTotEForElectron && 
	(momentumMagnitude > _minPForElectron)) { // basic electron ID, should be replaced by external general PID
      if (nsigd0 < _maxD0SigForElectron && nsigz0 < _maxZ0SigForElectron) {   // contraint to primary vertex
	mva_electron = _readers[0]->EvaluateMVA( "MLP method"           );
      }
    }
    if (charge != 0 &&
	        totalCalEnergy/momentumMagnitude < _maxEOverPForMuon &&
	(momentumMagnitude > _minPForMuon)) { // basic muon ID, should be replaced by external general PID
      if (TMath::Abs(nsigd0) < _maxD0SigForMuon && TMath::Abs(nsigz0) < _maxZ0SigForMuon) {  // contraint to primary vertex
	if (_use_yoke_for_muon && yokeEnergy > _minEyokeForMuon) {
	  mva_muon = _readers[1]->EvaluateMVA( "MLP method"           );
	}
	else {   // temporarily, provide this option for muon ID without using energy in Yoke; default option for now before problems get fixed
	  mva_muon = _readers[1]->EvaluateMVA( "MLP method"           );
	}
      }
    }
    /*if (charge != 0 && 
	totalCalEnergy/momentumMagnitude < _maxEOverPForMuon && 
	yokeEnergy > _minEyokeForMuon && 
	(momentumMagnitude > _minPForMuon)) { // basic muon ID, should be replaced by external general PID
      if (nsigd0 < _maxD0SigForMuon && nsigz0 < _maxZ0SigForMuon) {  // contraint to primary vertex
	mva_muon = _readers[1]->EvaluateMVA( "MLP method"           );
      }
      }*/
    // use output tagging for isolation requirement
    // default option to select the lepton with largest tagging
    if (mva_electron > _mvaCutForElectron) {
      if (!_is_one_isolep) {
	isoLeptons.push_back(recPart);
	isoLepTagging.push_back(mva_electron);
	_lep_type = 11;
	isoLepType.push_back(_lep_type);
      }
      else {
	if (mva_electron > _mva_lep_max) {
	  _lep_type = 11;
	  _mva_lep_max = mva_electron;
	  isoLeptons.clear();
	  isoLeptons.push_back(recPart);
	}
      }
    }
    if (mva_muon > _mvaCutForMuon) {
      if (!_is_one_isolep) {
   	isoLeptons.push_back(recPart);
	isoLepTagging.push_back(mva_muon);
	_lep_type = 13;
	isoLepType.push_back(_lep_type);
      }
      else {
	if (mva_muon > _mva_lep_max) {
	  _lep_type = 13;
	  _mva_lep_max = mva_muon;
	  isoLeptons.clear();
	  isoLeptons.push_back(recPart);
	}
      }
    }
  }
  //  Int_t nleptons = isoLeptons.size();

  //  if (nleptons < 1) throw marlin::SkipEventException(this);

  LCCollectionVec *pPFOsWithoutIsoLepCollection = new LCCollectionVec(LCIO::RECONSTRUCTEDPARTICLE);
  LCCollectionVec *pIsoLepCollection = new LCCollectionVec(LCIO::RECONSTRUCTEDPARTICLE);
  pPFOsWithoutIsoLepCollection->setSubset(true);
  pIsoLepCollection->setSubset(true);
  // save the selected lepton to a new collection
  for (std::vector<lcio::ReconstructedParticle*>::const_iterator iObj=isoLeptons.begin();iObj<isoLeptons.end();++iObj) {
    pIsoLepCollection->addElement(*iObj);
  }
  // save other PFOs to a new collection
  for (std::vector<lcio::ReconstructedParticle*>::const_iterator iObj=newPFOs.begin();iObj<newPFOs.end();++iObj) {
    Bool_t isLep=kFALSE;
    for (std::vector<lcio::ReconstructedParticle*>::const_iterator iLep=isoLeptons.begin();iLep<isoLeptons.end();++iLep) {
      if ((*iObj) == (*iLep)) isLep = kTRUE;
    }
    if (!isLep) pPFOsWithoutIsoLepCollection->addElement(*iObj);
  }
  // set the isolated lepton tagging as the collection parameter
  if (_is_one_isolep) {
    // save the largest one
    pIsoLepCollection->parameters().setValue( "ISOLepTagging", _mva_lep_max );
    pIsoLepCollection->parameters().setValue( "ISOLepType", _lep_type );
  }
  else {
    // save a vector of tagging
    pIsoLepCollection->parameters().setValues( "ISOLepTagging", isoLepTagging );
    pIsoLepCollection->parameters().setValues( "ISOLepType", isoLepType );

  }
  // add new collections
  evt->addCollection(pPFOsWithoutIsoLepCollection,_colNewPFOs.c_str());
  evt->addCollection(pIsoLepCollection,_colLeptons.c_str());


}



void myIsolatedLeptonTaggingProcessor::check( LCEvent * evt ) { 
}


void myIsolatedLeptonTaggingProcessor::end(){ 
  
  for (std::vector<TMVA::Reader*>::const_iterator ireader=_readers.begin();ireader!=_readers.end();++ireader) {
    delete *ireader;
  }

}