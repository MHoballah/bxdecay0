// Copyright 2017 François Mauger <mauger@lpccaen.in2p3.fr>

// Standard library:
#include <iostream>
#include <fstream>
#include <iomanip>
#include <math.h> 
// This project:
#include <bxdecay0/std_random.h>
#include <bxdecay0/dbd_gA.h>
#include <bxdecay0/event.h>


int main()
{
  std::random_device rd;
  //unsigned int seed = 42;
  std::default_random_engine generator(rd());
  bxdecay0::std_random prng(generator);

  bxdecay0::dbd_gA gA_generator;
  gA_generator.set_nuclide("Se82");
  gA_generator.set_process(bxdecay0::dbd_gA::PROCESS_G0);
  //gA_generator.set_shooting(bxdecay0::dbd_gA::SHOOTING_REJECTION);
  gA_generator.set_shooting(bxdecay0::dbd_gA::SHOOTING_INVERSE_TRANSFORM_METHOD);
  gA_generator.initialize();
  gA_generator.print(std::clog, "gA DBD generator", "[info] ");

  {
    std::ofstream fout1("bxdecay0_ex05_1.data");
    std::ofstream fout2("bxdecay0_ex05_2.data");
    std::ofstream fout3("bxdecay0_ex05_3.data");
    std::ofstream fout4("bxdecay0_ex05_4.data");
    //std::ofstream fout5("bxdecay0_ex05_5.data");

    unsigned int numb = 10000000;
    double emass=0.510999;

    for(unsigned int i=0; i < numb; i++){
      double e1;
      double e2;
      double cos12;
      gA_generator.shoot_e1_e2(prng, e1, e2);
      double p1 = std::sqrt(e1 * (e1 + 2. * emass));
      double b1 = p1 / (e1 + emass);
      double p2 = std::sqrt(e2 * (e2 + 2. * emass));
      double b2 = p2 / (e2 + emass);
      bxdecay0::event decay;
      gA_generator.shoot_cos_theta(prng, e1, e2, cos12, decay);
      fout1 << e1 <<  std::endl;
      fout2 <<  e2 <<  std::endl;
      double esum = e1 + e2;
      fout3 <<  esum << std::endl;
      fout4 << cos12 << std::endl;
      if(isnan(e1) || isnan(e2)) throw std::logic_error("PROBLEM! e1 or e2 is undefined " + std::to_string(e1) + "  " + std::to_string(e2)  + "  " + std::to_string(cos12) + "  " + std::to_string(i));
    //fout5 <<  b1 << " " << b2 << std::endl;
    //bxdecay0::dbd_gA::export_to_event(prng, e1, e2, cos12, decay); // Not implemented yet
    //decay.print(std::cerr, "DBD-gA event:", "[debug] ");
    }
  }
  
  gA_generator.reset();
  return 0;
}
