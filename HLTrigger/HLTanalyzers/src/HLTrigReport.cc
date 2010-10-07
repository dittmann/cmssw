/** \class HLTrigReport
 *
 * See header file for documentation
 *
 *  $Date: 2010/10/07 13:39:08 $
 *  $Revision: 1.15 $
 *
 *  \author Martin Grunewald
 *
 */

#include "HLTrigger/HLTanalyzers/interface/HLTrigReport.h"

#include "DataFormats/Common/interface/Handle.h"
#include "DataFormats/Common/interface/TriggerResults.h"

#include "FWCore/Common/interface/TriggerNames.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "Math/QuantFuncMathCore.h"

#include <iomanip>

//
// constructors and destructor
//
HLTrigReport::HLTrigReport(const edm::ParameterSet& iConfig) :
  hlTriggerResults_ (iConfig.getParameter<edm::InputTag> ("HLTriggerResults")),
  nEvents_(0),
  nWasRun_(0),
  nAccept_(0),
  nErrors_(0),
  hlWasRun_(0),
  hltL1s_(0),
  hltPre_(0),
  hlAccept_(0),
  hlErrors_(0),
  posL1s_(0),
  posPre_(0),
  hlNames_(0),
  hlIndex_(0),
  hlAccTotDS_(0),
  datasetNames_(0),
  datasetContents_(0),
  isCustomDatasets_(false),
  dsIndex_(0),
  dsAccTotS_(0),
  streamNames_(0),
  streamContents_(0),
  isCustomStreams_(false),
  refPath_("HLTFinalPath"),
  refIndex_(0),
  refRate_(1.0),
  hltConfig_()
{
 
  const edm::ParameterSet customDatasets(iConfig.getUntrackedParameter<edm::ParameterSet>("CustomDatasets",edm::ParameterSet()));
  isCustomDatasets_ = (customDatasets != edm::ParameterSet());
  if (isCustomDatasets_) {
    datasetNames_ = customDatasets.getParameterNamesForType<std::vector<std::string> >();
    for (std::vector<std::string>::const_iterator name = datasetNames_.begin(); name != datasetNames_.end(); name++) {
      datasetContents_.push_back(customDatasets.getParameter<std::vector<std::string> >(*name));
    }
  }

  const edm::ParameterSet customStreams (iConfig.getUntrackedParameter<edm::ParameterSet>("CustomStreams" ,edm::ParameterSet()));
  isCustomStreams_  = (customStreams  != edm::ParameterSet());
  if (isCustomStreams_ ) {
    streamNames_ = customStreams.getParameterNamesForType<std::vector<std::string> >();
    for (std::vector<std::string>::const_iterator name = streamNames_.begin(); name != streamNames_.end(); name++) {
      streamContents_.push_back(customStreams.getParameter<std::vector<std::string> >(*name));
    }
  }

  refPath_ = iConfig.getUntrackedParameter<std::string>("ReferencePath","HLTFinalPath");
  refRate_ = iConfig.getUntrackedParameter<double>("ReferenceRate",1.0);
  refIndex_= 0;

  LogDebug("HLTrigReport") << "HL TiggerResults: " + hlTriggerResults_.encode();
}

HLTrigReport::~HLTrigReport()
{ }

//
// member functions
//

void
HLTrigReport::beginRun(edm::Run const & iRun, edm::EventSetup const& iSetup)
{
  using namespace std;
  using namespace edm;
  
  bool changed (true);
  if (hltConfig_.init(iRun,iSetup,hlTriggerResults_.process(),changed)) {
    if (changed) {
      // dump previous
      dumpReport();
      nEvents_=0;
      nWasRun_=0;
      nAccept_=0;
      nErrors_=0;
      // const edm::TriggerNames & triggerNames = iEvent.triggerNames(*HLTR);
      hlNames_=hltConfig_.triggerNames();
      const unsigned int n(hlNames_.size());
      hlWasRun_.resize(n);
      hltL1s_.resize(n);
      hltPre_.resize(n);
      hlAccept_.resize(n);
      hlAccTot_.resize(n);
      hlErrors_.resize(n);
      posL1s_.resize(n);
      posPre_.resize(n);
      for (unsigned int i=0; i!=n; ++i) {
	hlWasRun_[i]=0;
	hltL1s_[i]=0;
	hltPre_[i]=0;
	hlAccept_[i]=0;
        hlAccTot_[i]=0;
	hlErrors_[i]=0;
	posL1s_[i]=-1;
	posPre_[i]=-1;
	const std::vector<std::string>& moduleLabels(hltConfig_.moduleLabels(i));
	for (unsigned int j=0; j<moduleLabels.size(); ++j) {
	  if (hltConfig_.moduleType(moduleLabels[j])=="HLTLevel1GTSeed") {
	    posL1s_[i]=j;
	  }
	  if (hltConfig_.moduleType(moduleLabels[j])=="HLTPrescaler"   ) {
	    posPre_[i]=j;
	  }
	}
      }

      // fill the matrices of hlIndex_, hlAccTotDS_ and dsIndex_, dsAccTotS_
      hlIndex_.clear();
      hlAccTotDS_.clear();
      dsAccTotS_.clear();
      if(!isCustomDatasets_){
        datasetNames_ = hltConfig_.datasetNames();
        datasetContents_ = hltConfig_.datasetContents();
      }
      if(!isCustomStreams_){
        streamNames_ = hltConfig_.streamNames();
        streamContents_ = hltConfig_.streamContents();
      }
      for(unsigned int ds=0; ds<datasetNames_.size(); ds++){
        hlIndex_.push_back(vector<unsigned int>());
        hlAccTotDS_.push_back(vector<unsigned int>());
        for(unsigned int p=0; p<datasetContents_[ds].size(); p++){
          unsigned int i = hltConfig_.triggerIndex(datasetContents_[ds][p]);
          if(i<n){
            hlIndex_[ds].push_back(i);
            hlAccTotDS_[ds].push_back(0);
          }
        }
      }   
      for(unsigned int s=0; s<streamNames_.size(); s++){
        dsIndex_.push_back(vector<unsigned int>());
        dsAccTotS_.push_back(vector<unsigned int>());
        for(unsigned int ds=0; ds<streamContents_[s].size(); ds++){
          unsigned int i = 0;
          for(; i<datasetNames_.size(); i++) if(datasetNames_[i] == streamContents_[s][ds]) break;
          if(i<datasetNames_.size() && hlIndex_[i].size()>0){ // report only datasets that have at least one path otherwise crash
            dsIndex_[s].push_back(i);
            dsAccTotS_[s].push_back(0);
          }
        }
      }
      refIndex_ = hltConfig_.triggerIndex(refPath_);
      if(refIndex_ >= n) refIndex_ = 0;
    }
  } else {
    // dump previous
    dumpReport();
    // clear
    nEvents_=0;
    nWasRun_=0;
    nAccept_=0;
    nErrors_=0;
    hlWasRun_.clear();
    hltL1s_.clear();
    hltPre_.clear();
    hlAccept_.clear();
    hlAccTot_.clear();
    hlErrors_.clear();
    posL1s_.clear();
    posPre_.clear();
    hlNames_.clear();
    hlIndex_.clear();
    hlAccTotDS_.clear();
    dsIndex_.clear();
    dsAccTotS_.clear();
  }
  return;
}
      
    
// ------------ method called to produce the data  ------------
void
HLTrigReport::analyze(const edm::Event& iEvent, const edm::EventSetup& iSetup)
{
  // accumulation of statistics event by event

  using namespace std;
  using namespace edm;

  nEvents_++;

  // get hold of TriggerResults
  Handle<TriggerResults> HLTR;
  iEvent.getByLabel(hlTriggerResults_,HLTR);
  if (HLTR.isValid()) {
    if (HLTR->wasrun()) nWasRun_++;
    const bool accept(HLTR->accept());
    LogDebug("HLTrigReport") << "HL TriggerResults decision: " << accept;
    if (accept) ++nAccept_;
    if (HLTR->error() ) nErrors_++;
  } else {
    LogDebug("HLTrigReport") << "HL TriggerResults with label ["+hlTriggerResults_.encode()+"] not found!";
    nErrors_++;
    return;
  }

  // decision for each HL algorithm
  const unsigned int n(hlNames_.size());
  bool acceptedByPrevoiusPaths = false;
  for (unsigned int i=0; i!=n; ++i) {
    if (HLTR->wasrun(i)) hlWasRun_[i]++;
    if (HLTR->accept(i)){
      acceptedByPrevoiusPaths = true;
      hlAccept_[i]++;
    }
    if (acceptedByPrevoiusPaths) hlAccTot_[i]++;
    if (HLTR->error(i) ) hlErrors_[i]++;
    const int index(static_cast<int>(HLTR->index(i)));
    if (HLTR->accept(i)) {
      if (index>=posL1s_[i]) hltL1s_[i]++;
      if (index>=posPre_[i]) hltPre_[i]++;
    } else {
      if (index> posL1s_[i]) hltL1s_[i]++;
      if (index> posPre_[i]) hltPre_[i]++;
    }
  }

  // calculate accumulation of accepted events by a path within a dataset
  std::vector<bool> acceptedByDS(hlIndex_.size(), false);
  for (size_t ds=0; ds<hlIndex_.size(); ++ds){
    for (size_t p=0; p<hlIndex_[ds].size(); ++p){
      if (acceptedByDS[ds] || HLTR->accept(hlIndex_[ds][p])){
        acceptedByDS[ds] = true;
        hlAccTotDS_[ds][p]++;
      }
    }
  }

  // calculate accumulation of accepted events by a dataset within a stream
  for (size_t s=0; s<dsIndex_.size(); ++s){
    bool acceptedByS = false;
    for (size_t ds=0; ds<dsIndex_[s].size(); ++ds){
      if (acceptedByS || acceptedByDS[dsIndex_[s][ds]]){
        acceptedByS = true;
        dsAccTotS_[s][ds]++;
      }
    }
  }

  return;

}

void
HLTrigReport::endJob()
{
  dumpReport();
  return;
}

void
HLTrigReport::dumpReport()
{
  // final printout of accumulated statistics

  using namespace std;
  using namespace edm;
  const unsigned int n(hlNames_.size());

  if ((n==0) && (nEvents_==0)) return;

  double scale = hlAccept_[refIndex_]>0 ? refRate_/hlAccept_[refIndex_] : 0.;
  double alpha = 1 - (1.0 - .6854)/2; // for the Clopper-Pearson 68% CI

  LogVerbatim("HLTrigReport") << dec << endl;
  LogVerbatim("HLTrigReport") << "HLT-Report " << "---------- Event  Summary ------------" << endl;
  LogVerbatim("HLTrigReport") << "HLT-Report"
	 << " Events total = " << nEvents_
	 << " wasrun = " << nWasRun_
	 << " passed = " << nAccept_
	 << " errors = " << nErrors_
	 << endl;

  LogVerbatim("HLTrigReport") << endl;
  LogVerbatim("HLTrigReport") << "HLT-Report " << "---------- HLTrig Summary ------------" << endl;
  LogVerbatim("HLTrigReport") << "HLT-Report "
	 << right << setw(7) << "HLT #" << " "
	 << right << setw(7) << "WasRun" << " "
	 << right << setw(7) << "L1S" << " "
	 << right << setw(7) << "Pre" << " "
	 << right << setw(7) << "HLT" << " "
	 << right << setw(9) << "%L1sPre" << " "
         << right << setw(7) << "HLTtot" << " "
         << right << setw(7) << "Rate" << " "
         << right << setw(7) << "RateHi" << " "
         << right << setw(7) << "RateTot" << " "
	 << right << setw(7) << "Errors" << " "
	 << "Name" << endl;

  if (n>0) {
    for (unsigned int i=0; i!=n; ++i) {
      LogVerbatim("HLTrigReport") << "HLT-Report "
	   << right << setw(7) << i << " "
	   << right << setw(7) << hlWasRun_[i] << " "
	   << right << setw(7) << hltL1s_[i] << " "
	   << right << setw(7) << hltPre_[i] << " "
	   << right << setw(7) << hlAccept_[i] << " "
	   << static_cast<float>(100*hlAccept_[i])/
	      static_cast<float>(max(hltPre_[i],1u)) << " "
           << right << setw(7) << hlAccTot_[i] << " "
           << right << setw(7) << fixed << setprecision(1) << scale*hlAccept_[i] << " "
           << right << setw(7) << fixed << setprecision(1) << 
              ((hlAccept_[refIndex_]-hlAccept_[i] > 0) ? refRate_*ROOT::Math::beta_quantile(alpha, hlAccept_[i]+1, hlAccept_[refIndex_]-hlAccept_[i]) : 0) << " "
           << right << setw(7) << fixed << setprecision(1) << scale*hlAccTot_[i] << " "
	   << right << setw(9) << fixed << setprecision(5)
	   << right << setw(7) << hlErrors_[i] << " "
	   << hlNames_[i] << endl;
    }

    // now for each stream
    for (size_t s=0; s<dsIndex_.size(); ++s){
      LogVerbatim("HLTrigReport") << endl;
      LogVerbatim("HLTrigReport") << "HLT-Report " << "---------- Stream Summary: " << streamNames_[s] << " ------------" << endl;
      LogVerbatim("HLTrigReport") << "HLT-Report "
         << right << setw(10) << "Dataset #" << " "
         << right << setw(10) << "Individual" << " "
         << right << setw(10) << "Total" << " "
         << right << setw(10) << "Rate" << " "
         << right << setw(10) << "RateHi" << " "
         << right << setw(10) << "RateTot" << " "
         << "Name" << endl;
      for (size_t ds=0;ds<dsIndex_[s].size(); ++ds){
        unsigned int acceptedDS = hlAccTotDS_[dsIndex_[s][ds]][hlIndex_[dsIndex_[s][ds]].size()-1];
        LogVerbatim("HLTrigReport") << "HLT-Report "
           << right << setw(10) << ds << " "
           << right << setw(10) << acceptedDS << " "
           << right << setw(10) << dsAccTotS_[s][ds] << " "
           << right << setw(10) << fixed << setprecision(1) << scale*acceptedDS << " "
           << right << setw(10) << fixed << setprecision(1) << 
              ((hlAccept_[refIndex_]-acceptedDS > 0) ? refRate_*ROOT::Math::beta_quantile(alpha, acceptedDS+1, hlAccept_[refIndex_]-acceptedDS) : 0) << " "
           << right << setw(10) << fixed << setprecision(1) << scale*dsAccTotS_[s][ds] << " "
           << datasetNames_[dsIndex_[s][ds]] << endl;
      }
    }

    // now for each dataset
    for (size_t ds=0; ds<hlIndex_.size(); ++ds){
      LogVerbatim("HLTrigReport") << endl;
      LogVerbatim("HLTrigReport") << "HLT-Report " << "---------- Dataset Summary: " << datasetNames_[ds] << " ------------" << endl;
      LogVerbatim("HLTrigReport") << "HLT-Report "
         << right << setw(7) << "HLT #" << " "
         << right << setw(7) << "WasRun" << " "
         << right << setw(7) << "L1S" << " "
         << right << setw(7) << "Pre" << " "
         << right << setw(7) << "HLT" << " "
         << right << setw(7) << "HLTtot" << " "
         << right << setw(7) << "Rate" << " "
         << right << setw(7) << "RateHi" << " "
         << right << setw(7) << "RateTot" << " "
         << right << setw(9) << "%L1sPre" << " "
         << right << setw(7) << "Errors" << " "
         << "Name" << endl;  
      for (size_t p=0; p<hlIndex_[ds].size(); ++p){
        LogVerbatim("HLTrigReport") << "HLT-Report "
           << right << setw(7) << p << " "
           << right << setw(7) << hlWasRun_[hlIndex_[ds][p]] << " "
           << right << setw(7) << hltL1s_[hlIndex_[ds][p]] << " "
           << right << setw(7) << hltPre_[hlIndex_[ds][p]] << " "
           << right << setw(7) << hlAccept_[hlIndex_[ds][p]] << " "
           << right << setw(7) << hlAccTotDS_[ds][p] << " "
           << right << setw(7) << fixed << setprecision(1) << scale*hlAccept_[hlIndex_[ds][p]] << " "
           << right << setw(7) << fixed << setprecision(1) <<
              ((hlAccept_[refIndex_]-hlAccept_[hlIndex_[ds][p]] > 0) ? refRate_*ROOT::Math::beta_quantile(alpha, hlAccept_[hlIndex_[ds][p]]+1, hlAccept_[refIndex_]-hlAccept_[hlIndex_[ds][p]]) : 0) << " "
           << right << setw(7) << fixed << setprecision(1) << scale*hlAccTotDS_[ds][p] << " "
           << right << setw(9) << fixed << setprecision(5)
           << static_cast<float>(100*hlAccept_[hlIndex_[ds][p]])/
              static_cast<float>(max(hltPre_[hlIndex_[ds][p]],1u)) << " "
           << right << setw(7) << hlErrors_[hlIndex_[ds][p]] << " "
           << hlNames_[hlIndex_[ds][p]] << endl;
      }
    }
  } else {
    LogVerbatim("HLTrigReport") << "HLT-Report - No HLT paths found!" << endl;
  }

  LogVerbatim("HLTrigReport") << endl;
  LogVerbatim("HLTrigReport") << "HLT-Report end!" << endl;
  LogVerbatim("HLTrigReport") << endl;

  return;
}
