#include <iostream>
#include <fstream>
#include <cmath>
#include <vector>
#include <string>

#include <TROOT.h>
#include <TStyle.h>
#include <TH1F.h>
#include <TF1.h>
#include <TCanvas.h>
#include <TStyle.h>
#include <TGraphErrors.h>

void analysis()
{
  ifstream InputFile, InputFile1, InputFile2, InputFile3, InputFile_beta, InputFile_E1, InputFile_E2;
  InputFile.open ("bxdecay0_ex05_1.data");
  InputFile1.open ("bxdecay0_ex05_2.data");
  InputFile2.open ("bxdecay0_ex05_3.data");
  InputFile3.open ("bxdecay0_ex05_4.data");
  InputFile_beta.open ("Beta.data");
  InputFile_E1.open ("Prob_sum_E1.data");
  InputFile_E2.open ("Prob_sum_E2.data");
  
  if(InputFile.is_open() && InputFile1.is_open()&& InputFile2.is_open() && InputFile3.is_open() && InputFile_beta.is_open() && InputFile_E1.is_open() && InputFile_E2.is_open())
    cout << "Files Opened Succefully! " << endl;
  else cerr << " Problem Openening Files!" << endl;
  
  TH1D *e_1 = new TH1D("simulated E1", "simulated E1", 100,-1,4);
  e_1->Sumw2();
  TH1D *e_2 = new TH1D("simulated E2", "simulated E2", 100,-1,4);
  e_2->Sumw2();
  TH1D *e_sum = new TH1D("simulated Esum", "simulated Esum", 100,-1,4);
  e_sum->Sumw2();
  TH1D *cos_12 = new TH1D("cos12", "cos12", 100,-2,2);
  cos_12->Sumw2();
  TH1F *beta_plot = new TH1F("beta", "beta1*beta2", 100,-2,2);
  beta_plot->Sumw2();
  TH1D *en_1 = new TH1D("E1", " all E1 values", 100, -1, 4);
  en_1->Sumw2();
  TH1D *en_2 = new TH1D("E2", " all E2 values", 100, -1, 4);
  en_2->Sumw2();
  TH1D *en_sum = new TH1D("Esum", "Esum", 100, -1,4);
  en_sum->Sumw2();



  TCanvas *plots = new TCanvas("plots", "plots");
  gStyle->SetOptStat(kFALSE);
  
  long double e1, e2, esum, cos12,  prob_1, prob_2, proba;
  int count =0;
  while(InputFile.good())
    {
      InputFile >> e1;
      InputFile1 >> e2;
      InputFile2 >> esum;
      InputFile3 >> cos12;
      e_1->Fill(e1);
      e_2->Fill(e2);
      e_sum->Fill(esum);
      cos_12->Fill(cos12);
      count ++;
      if(count == 1) cout << e1 << endl;
    }

  double beta, beta_add = 0, proba_add = 0;
  unsigned int beta_count = 0;
  while(InputFile_beta.good())
    {
      InputFile_beta >> beta >> proba;
      beta_add = beta_add + (beta)*proba;
      proba_add = proba_add + proba;
    }
  TF1 *beta_ft = new TF1("beta_ft", "1-[0]*x", -2, 2);
  cout << " fit " << beta_add/proba_add << endl;
  beta_ft->SetParameter(0,beta_add/proba_add);
  beta_plot->FillRandom("beta_ft", 5000);
  
  while(InputFile_E1.good()){
    InputFile_E1 >> e1 >> prob_1;
    en_1->Fill(e1,prob_1);
    InputFile_E2 >> e2 >> prob_2;
    en_2->Fill(e2, prob_2);
    //cout << e1 << "  " << e2 << "  " << prob_1 << "  " << prob_2 << endl;
    if(e1+e2 > 2.998) continue;
    en_sum->Fill(e2+e1, prob_1 * prob_2);
    }
 

  gStyle->SetOptStat("nemr");
  
  cout << count << endl;
  plots->Divide(2,2);
  plots->cd(1);
  e_1->SetXTitle("E1 [MeV]");
  e_1->SetYTitle("Count");
  e_1->Scale(1.0/e_1->Integral());
  e_1->Draw();
  en_1->SetLineColor(kRed);
  en_1->Scale(1.0/en_1->Integral());
  en_1->Draw("same");
  plots->cd(2);
  e_2->SetXTitle("E2 [MeV]");
  e_2->SetYTitle("Count");
  e_2->Scale(1.0/e_2->Integral());
  e_2->Draw();
  en_2->SetLineColor(kRed);
  en_2->Scale(1.0/en_2->Integral());
  en_2->Draw("same");
  plots->cd(3);
  e_sum->SetXTitle("E1 + E2 [MeV]");
  e_sum->SetYTitle("Count");
  e_sum->Scale(1.0/e_sum->Integral());
  e_sum->Draw();
  en_sum->SetLineColor(kRed);
  en_sum->Scale(1.0/en_sum->Integral());
  en_sum->Draw("same");
  plots->cd(4);
  cos_12->SetXTitle("cos12");
  cos_12->SetYTitle("Count");
  //cos_12->Fit("pol1");
  cos_12->Scale(1.0/cos_12->Integral());
  cos_12->Draw();
  beta_plot->SetLineColor(kRed);
  beta_plot->Scale(1.0/beta_plot->Integral());
  beta_plot->Draw("same");
}