/*
 
Copyright (C) 2007 Jindrich Kolorenc, Michal Bajdich

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 
*/



#include "Qmc_std.h"
#include "MO_matrix_bspline.h"
#include "Sample_point.h"
#include "qmc_io.h"

//--------------------------------------------------------------------------

void MO_matrix_bspline::init() {

  single_write(cout, "Bspline MO\n");
  Einspline.Resize(nmo);
  if(complexspline)
    EinsplineComplex.Resize(nmo);
  else
    EinsplineReal.Resize(nmo);


  for(int m=0; m < nmo; m++) {
    string orbfile_0=valfiles[m];
    ifstream ORB_0(orbfile_0.c_str());
    if(!ORB_0){
      error("couldn't find orb file ", orbfile_0);
    } 
   
    if(complexspline){
      EinsplineComplex[m]=new EinsplineOrbComplex;
      Einspline[m]=EinsplineComplex[m];
    }
    else{
      EinsplineReal[m]=new EinsplineOrbReal;
      Einspline[m]=EinsplineReal[m];
    }

    Einspline[m]->setperiodic(periodic);
    if(periodic){
      Einspline[m]->setRecipLattice(RecipLatVec);
      Einspline[m]->setBounds(LatVec);
      Einspline[m]->kpoint(kpoint);
      Einspline[m]->setorigin(origin);
    }

    single_write(cout,"Reading Orbfile: ",orbfile_0,"\n"); 
    Einspline[m]->read(ORB_0);
    ORB_0.close();
  }
}


//----------------------------------------------------------------------


void MO_matrix_bspline::buildLists(Array1 < Array1 <int> > & occupations) {
  int numlists=occupations.GetDim(0);
  moLists.Resize(numlists);
  for(int lis=0; lis < numlists; lis++) {
    int nmo_list=occupations(lis).GetDim(0);
    moLists(lis).Resize(nmo_list);
    for(int mo=0; mo < nmo_list; mo++) {
      moLists(lis)(mo)=occupations(lis)(mo);
    }
  }
  
}

//----------------------------------------------------------------------

int MO_matrix_bspline::showinfo(ostream & os)
{
  os << "Bspline Molecular Orbital\n";
  string indent="  ";
  os << "Using orbital value plotfiles : \n";
  Array1 <doublevar> origin, box_size, spacing;
  for(int i=0;i<valfiles.size();i++){
    os << indent << valfiles[i] <<endl;
    EinsplineComplex[i]->getorigin(origin);
    EinsplineComplex[i]->getbox_size(box_size);
    EinsplineComplex[i]->getspacing(spacing);
    os << indent <<"origin:   "<<origin(0)
                         <<"  "<<origin(1)
                         <<"  "<<origin(2)<<endl;
    os << indent <<"box_size: "<<box_size(0)
                         <<"  "<<box_size(1)
                         <<"  "<<box_size(2)<<endl;
    os << indent <<"spacing:  "<<spacing(0)
                         <<"  "<<spacing(1)
                         <<"  "<<spacing(2)<<endl;
    
  }
  os << "Number of molecular orbitals: " << nmo << endl;
  
  return 1;
}

int MO_matrix_bspline::writeinput(string & indent, ostream & os)
{
  os << indent << "BSPLINE_MO" << endl;
  os << indent << "NMO " << nmo << endl;
  os << indent << "MAGNIFY " << magnification_factor << endl;
  string indent2=indent+"  ";
  os << indent2 << "VALPLT {" <<endl;

  for(int i=0;i<valfiles.size();i++)
    os << indent2 << valfiles[i] <<endl; 

  os << indent2 << "}"<<endl;
  os << indent << "}" << endl;
  return 1;
}

void MO_matrix_bspline::read(vector <string> & words, unsigned int & startpos, System * sys){
   unsigned int pos=startpos;

   if(!readvalue(words, pos, nmo, "NMO"))
     {
       error("Need NMO in molecular orbital section");
     }
   
   if(nmo > 40000) 
     error("You have entered more than 40,000 for NMO.  This seems a bit big; we most likely"
	   " can't handle it.");
   
   
   pos=0;
   if(!readvalue(words, pos, magnification_factor, "MAGNIFY")) {
     magnification_factor=1;
   }
   pos=0;

   pos=0;
   if(readsection(words, pos, valfiles, "VALPLT")) {
     if(valfiles.size()!=nmo){
       cout << "#plotfiles " << valfiles.size()<<" inside VALPLT" << endl;
       error("Must have the same number as NMO");
     }
   }
   else{
     error("Need VALPLT section in BSPLINE_MO ORBITALS");
   }

   complexspline=0;
   periodic=0; 

   origin.Resize(3);
   LatVec.Resize(3,3);
   
   if(sys->getBounds(LatVec)){
     periodic=1;
     single_write(cout," Using periodic");
     if(!sys->getRecipLattice(RecipLatVec))
       error("Could not read Reciprocal Lattice Vectors from system");
     sys->kpoint(kpoint);
     for(int d=0;d<3;d++){
       if(kpoint(d)!=0){
	 //using complex splines if not in gamma point
	 complexspline=1;
	 break;
       }
     }
     origin.Resize(3);
     sys->getorigin(origin);
     if(complexspline)
       single_write(cout," and complex splines\n");
     else
       single_write(cout," and real splines\n");
   }
   init();
}

//------------------------------------------------------------------------

void MO_matrix_bspline::updateVal(Sample_point * sample, int e,
                                   int listnum,
                                   //!< which list to use
                                   Array2 <doublevar> & newvals
                                   //!< The return: in form (MO, val)
) {
  //cout <<"MO_matrix_bspline::updateVal"<<endl;
  newvals=0;
  int nmo_list=moLists(listnum).GetDim(0);
  Array1 <doublevar> xyz(3);
  doublevar val;
  sample->getElectronPos(e,xyz);
  Array1 <doublevar> Unit_pos_in_prim_latice(3);
  if(periodic){
    //calculate coordinates in the fractions of Latice vectors 
    for(int i=0;i<3;i++){
      Unit_pos_in_prim_latice(i)=0;
      for(int j=0;j<3;j++)
	Unit_pos_in_prim_latice(i)+=(xyz(j)-origin(j))*RecipLatVec(i,j);
    }
  }

  for(int m=0; m < nmo_list; m++) {
     int mo=moLists(listnum)(m);
     if(!complexspline && !periodic){
       //evaluate spline
       Einspline[mo]->evaluate_spline (xyz, val);
     }
     else if(!complexspline && periodic){
       //evaluate spline
       Einspline[mo]->evaluate_spline (Unit_pos_in_prim_latice, val);
     }
     else if(complexspline && periodic){
       dcomplex cval;
       //evaluate spline
       Einspline[mo]->evaluate_spline (Unit_pos_in_prim_latice, cval);
       //multiply by e(ipikr) to get original values 
       dcomplex eikr=exp(I*pi*dot(Unit_pos_in_prim_latice, kpoint));
       cval *= eikr;
       val=cval.real();
     }
     else if(complexspline && !periodic){ 
       dcomplex cval;
       //evaluate spline
       Einspline[mo]->evaluate_spline (xyz, cval);
       //make it real for now
       val=cval.real();
     }
     else{
       error("dont know what to do!");
     }
     //multiply by  magnification_factor and store to newvals;
     newvals(mo,0)= magnification_factor*val;
  }
}

//------------------------------------------------------------------------


void MO_matrix_bspline::updateLap(Sample_point * sample, int e,
				  int listnum,
				  //!< which list to use
				  Array2 <doublevar> & newvals
				  //!< The return: in form (MO, [val, grad, lap])
) {
  //cout <<"MO_matrix_bspline::updateLap"<<endl;
  newvals=0;
  doublevar val, lap;
  Array1 <doublevar> grad(3);
  Array1 <doublevar> xyz(3),  pos_in_prim_latice(3);
  int nmo_list=moLists(listnum).GetDim(0);
  sample->getElectronPos(e,xyz);
  Array1 <doublevar> Unit_pos_in_prim_latice(3);
  if(periodic){
    //calculate coordinates in the fractions of Latice vectors 
    for(int i=0;i<3;i++){
      Unit_pos_in_prim_latice(i)=0;
      for(int j=0;j<3;j++)
	Unit_pos_in_prim_latice(i)+=(xyz(j)-origin(j))*RecipLatVec(i,j);
    }
  }

  for(int m=0; m < nmo_list; m++) {
     int mo=moLists(listnum)(m);

     if(!complexspline && !periodic){
       //evaluate spline
       Einspline[mo]->evaluate_spline (xyz, val, grad, lap);
     }
     else if(!complexspline && periodic){
       //evaluate spline
       doublevar val_spline;
       Array1 <doublevar> grad_spline(3);
       Array2 <doublevar> hess_spline(3,3);
       Einspline[mo]->evaluate_spline (Unit_pos_in_prim_latice, val_spline, grad_spline, hess_spline);

       //adjust the values for coordinate change
       val=val_spline;
       grad=0;
       for(int i=0;i<3;i++){
	 for(int j=0;j<3;j++){
	   grad(i)+=grad_spline(j)*RecipLatVec(j,i);
	 }
       }
       lap=0;
       for(int i=0;i<3;i++)
	 for(int k=0;k<3;k++)
	   for(int l=0;l<3;l++){
	     lap+=RecipLatVec(k,i)*RecipLatVec(l,i)*hess_spline(k,l);
	   }

     }
     else if(complexspline && periodic){
       //evaluate spline;
       dcomplex  cval_spline,clap;
       Array1 <dcomplex> cgrad_spline(3),cgrad(3),xgrad(3);
       Array2 <dcomplex> chess_spline(3,3),  chess(3,3);
       Einspline[mo]->evaluate_spline (Unit_pos_in_prim_latice, cval_spline, cgrad_spline, chess_spline);

       //multiply by e(ipikr) to get original values 
       dcomplex eikr=exp(I*pi*dot(Unit_pos_in_prim_latice, kpoint));
       dcomplex cval = eikr*cval_spline;
       for(int d=0;d<3;d++)
	 cgrad(d)= eikr*(I*cval_spline*kpoint(d)*pi + cgrad_spline(d) );
       
       //adjust for coordinate change
       xgrad=dcomplex(0,0);
       for(int i=0;i<3;i++){
	 for(int j=0;j<3;j++){
	   xgrad(i)+=cgrad(j)*RecipLatVec(j,i);
	 }
       }

       for(int i=0;i<3;i++)
	 for(int j=0;j<3;j++)
	   chess(i,j)= eikr*( -kpoint(i)*kpoint(j)*cval_spline*pi*pi + I*pi*kpoint(i)*cgrad_spline(j)+I*pi*kpoint(j)*cgrad_spline(i)+ chess_spline(i,j));

       clap=0;
       for(int i=0;i<3;i++)
	 for(int k=0;k<3;k++)
	   for(int l=0;l<3;l++){
	     clap+=RecipLatVec(k,i)*RecipLatVec(l,i)*chess(k,l);
	   }
       
       val=cval.real();
       for(int d=0;d<3;d++)
	 grad(d)=xgrad(d).real();
       lap=clap.real();
     }
     else if(complexspline && !periodic){ 
        //evaluate spline;
       dcomplex  cval_spline,clap_spline;
       Array1 <dcomplex> cgrad_spline(3);
       Einspline[mo]->evaluate_spline (xyz, cval_spline, cgrad_spline, clap_spline);
       //make everything real for now, make change is the orbital will be complex;
       val=cval_spline.real();
       for(int i=0;i<3;i++)
	 grad(i)=cgrad_spline(i).real();
       lap=clap_spline.real();
     }
     else{
       error("dont know what to do!");
     }

     //multiply by  magnification_factor and store to newvals;
     newvals(m,0)= magnification_factor*val;
     for(int d=0;d<3;d++)
       newvals(m,d+1)= magnification_factor*grad(d);
     newvals(m,4)=magnification_factor*lap;
   }
  
}

//--------------------------------------------------------------------------


void MO_matrix_bspline::updateHessian(
  Sample_point * sample,
  int e,
  int listnum,
  //const Array1 <int> & occupation,
  //!<A list of the MO's to evaluate
  Array2 <doublevar> & newvals
  //!< The return: in form (MO, [val, grad, dxx,dyy,...])
)
{
   //cout <<"MO_matrix_bspline::updateHess"<<endl;
  newvals=0;
  doublevar val;
  Array2 <doublevar> hess(3,3);
  Array1 <doublevar> grad(3);
  Array1 <doublevar> xyz(3),  pos_in_prim_latice(3);
  int nmo_list=moLists(listnum).GetDim(0);
  sample->getElectronPos(e,xyz);
  Array1 <doublevar> Unit_pos_in_prim_latice(3);
  if(periodic){
    //calculate coordinates in the fractions of Latice vectors 
    for(int i=0;i<3;i++){
      Unit_pos_in_prim_latice(i)=0;
      for(int j=0;j<3;j++)
	Unit_pos_in_prim_latice(i)+=(xyz(j)-origin(j))*RecipLatVec(i,j);
    }
  }

  for(int m=0; m < nmo_list; m++) {
     int mo=moLists(listnum)(m);

     if(!complexspline && !periodic){
       //evaluate spline
       Einspline[mo]->evaluate_spline (xyz, val, grad, hess);
     }
     else if(!complexspline && periodic){
       //evaluate spline
       doublevar val_spline;
       Array1 <doublevar> grad_spline(3);
       Array2 <doublevar> hess_spline(3,3);
       Einspline[mo]->evaluate_spline (Unit_pos_in_prim_latice, val_spline, grad_spline, hess_spline);

       //adjust the values for coordinate change
       val=val_spline;
       grad=0;
       for(int i=0;i<3;i++){
	 for(int j=0;j<3;j++){
	   grad(i)+=grad_spline(j)*RecipLatVec(j,i);
	 }
       }
       hess=0;
       for(int i=0;i<3;i++)
	 for(int j=0;j<3;j++)
	   for(int k=0;k<3;k++)
	     for(int l=0;l<3;l++){
	       hess(i,j)+=RecipLatVec(k,i)*RecipLatVec(l,j)*hess_spline(k,l);
	   }

     }
     else if(complexspline && periodic){
       //evaluate spline;
       dcomplex  cval_spline,clap;
       Array1 <dcomplex> cgrad_spline(3),cgrad(3),xgrad(3);
       Array2 <dcomplex> chess_spline(3,3),  chess(3,3);
       Einspline[mo]->evaluate_spline (Unit_pos_in_prim_latice, cval_spline, cgrad_spline, chess_spline);

       //multiply by e(ipikr) to get original values 
       dcomplex eikr=exp(I*pi*dot(Unit_pos_in_prim_latice, kpoint));
       dcomplex cval = eikr*cval_spline;
       for(int d=0;d<3;d++)
	 cgrad(d)= eikr*(I*cval_spline*kpoint(d)*pi + cgrad_spline(d) );
       
       //adjust for coordinate change
       xgrad=dcomplex(0,0);
       for(int i=0;i<3;i++){
	 for(int j=0;j<3;j++){
	   xgrad(i)+=cgrad(j)*RecipLatVec(j,i);
	 }
       }

       for(int i=0;i<3;i++)
	 for(int j=0;j<3;j++)
	   chess(i,j)= eikr*( -kpoint(i)*kpoint(j)*cval_spline*pi*pi + I*pi*kpoint(i)*cgrad_spline(j)+I*pi*kpoint(j)*cgrad_spline(i)+ chess_spline(i,j));

       
       Array2 <dcomplex> chess2(3,3);
       chess2=dcomplex(0,0);
       for(int i=0;i<3;i++)
	 for(int j=0;j<3;j++)
	   for(int k=0;k<3;k++)
	     for(int l=0;l<3;l++){
	       chess2(i,j)+=RecipLatVec(k,i)*RecipLatVec(l,j)*chess(k,l);
	     }
       
       val=cval.real();
       for(int d=0;d<3;d++)
	 grad(d)=xgrad(d).real();
       for(int i=0;i<3;i++)
	 for(int j=0;j<3;j++)
	   hess(i,j)=chess2(i,j).real();
     }
     else if(complexspline && !periodic){ 
       //evaluate spline;
       dcomplex  cval_spline;
       Array1 <dcomplex> cgrad_spline(3);
       Array2 <dcomplex> chess_spline(3,3);
       Einspline[mo]->evaluate_spline (xyz, cval_spline, cgrad_spline, chess_spline);
       //make everything real for now, make change is the orbital will be complex;
       val=cval_spline.real();
       for(int i=0;i<3;i++)
	 grad(i)=cgrad_spline(i).real();
       for(int i=0;i<3;i++)
	 for(int j=0;j<3;j++)
	   hess(i,j)=chess_spline(i,j).real(); 
     }
     else{
       error("dont know what to do!");
     }

     //multiply by  magnification_factor and store to newvals;
     int counter=0;
     newvals(m,counter++)= magnification_factor*val;
     for(int d=0;d<3;d++)
       newvals(m,counter++)= magnification_factor*grad(d);
     for(int i=0;i<3;i++)
       for(int j=i;j<3;j++)
	 newvals(m,counter++)=magnification_factor*hess(i,j);
   }
}
//--------------------------------------------------------------------------
