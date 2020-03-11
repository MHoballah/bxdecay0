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
  ifstream InputFile, InputFile1, InputFile2, InputFile3;
  InputFile.open ("bxdecay0_ex05_1.data");
  InputFile1.open ("bxdecay0_ex05_2.data");
  InputFile2.open ("bxdecay0_ex05_3.data");
  InputFile3.open ("bxdecay0_ex05_4.data");
  
  if(InputFile.is_open() && InputFile1.is_open()&& InputFile2.is_open() && InputFile3.is_open()) cout << "IS OPEN " << endl;
  
  TH1D *e_1 = new TH1D("E1", "E1", 100,-1,4);
  TH1D *e_2 = new TH1D("E2", "E2", 100,-1,4); 
  TH1D *e_sum = new TH1D("Esum", "Esum", 100,-1,4);
  TH1D *cos_12 = new TH1D("cos12", "cos12", 100,-2,2);
  
  long double e1, e2, esum, cos12;
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
  cout << e1 << endl;
  cout << count << endl;
  TCanvas *plots = new TCanvas("plots", "plots");
  plots->Divide(2,2);
  plots->cd(1);
  e_1->SetXTitle("E1 [MeV]");
  e_1->SetYTitle("Count");
  e_1->Draw();
  plots->cd(2);
  e_2->SetXTitle("E2 [MeV]");
  e_2->SetYTitle("Count");
  e_2->Draw();
  plots->cd(3);
  e_sum->SetXTitle("E1 + E2 [MeV]");
  e_sum->SetYTitle("Count");
  e_sum->Draw();
  plots->cd(4);
  cos_12->SetXTitle("cos12");
  cos_12->SetYTitle("Count");
  cos_12->Draw();

}
