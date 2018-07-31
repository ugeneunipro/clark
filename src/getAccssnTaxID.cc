/*
 * CLARK, CLAssifier based on Reduced K-mers.
 */

/*
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.

   Copyright 2013-2017, Rachid Ounit <clark.ucr.help at gmail.com>
 */

/*
 * @author: Rachid Ounit, Ph.D Candidate.
 * @project: CLARK, Metagenomic and Genomic Sequences Classification project.
 * @note: C++ IMPLEMENTATION supported on latest Linux and Mac OS.
 *
 */

#include <fstream>
#include <cstdlib>
#include <iostream>
#include <vector>
#include <cstring>
#include <iomanip>
#include <stdint.h>
#include "./file.hh"
#include "FILEex.h"
#include <map>
using namespace std;

struct seqData
{
	std::string 	Name;
	std::string	Accss; 
	size_t		offset;
	size_t		length;
	seqData():Name(""),Accss(""), offset(0), length(0)
	{}
};

int main(int argc, char** argv)
{
	if (argc != 3)
	{
        cerr << "UGENE-customized " << argv[0] << endl;
		cerr << "Usage: cat nucl_accession2taxid | "<< argv[0] << " <./file of filenames> <./merged.dmp>"<< endl;
		exit(-1);
	}
	FILE * oldTx = fopen(argv[2], "r");
        if (oldTx == NULL)
        {
                cerr << "Failed to open " << argv[2] << endl;
                exit(-1);
        }
	FILE * accToTx = stdin;
	FILE * meta_f = fopen(argv[1], "r");
	if (meta_f == NULL)
	{
		cerr << "Failed to open " << argv[1] << endl;
		exit(1);
	}
	///////////////////////////////////
	string line, file;
	vector<string> ele, eles;
        vector<char> sep, seps;
        sep.push_back('|');
	sep.push_back('.');
	sep.push_back('>');
	seps.push_back(' ');
	seps.push_back('\t');
	seps.push_back(':');
	vector<int> TaxIDs;
	map<std::string,uint32_t> accToidx; 
	vector<seqData> seqs;	
	map<std::string,uint32_t>::iterator it;
	uint32_t idx = 0, i_accss = 0;
	size_t offset = 0;
	size_t length = 0;
	string acc = "";
    cerr << "Loading accession number of all files... " << endl;

    size_t filesCounter = 1;

	while (getLineFromFile(meta_f, file))
    {
        cerr << filesCounter << ". \'" << file.c_str() << "\' processing... ";

		FILEex * fd = fopenEx(file.c_str(), "r");
		if (fd == NULL)
		{
			cerr << "Failed to open sequence file: " <<  file << endl;
			cout << file << "\tUNKNOWN" << endl;
			continue;
		}

		offset = 0;
		length = 0;
		bool fileContainsSequence = false;

        size_t foundSequencesCount = 0;
		while (getLineFromFile(fd, line))
        {
			if (line[0] != '>') {
				length += line.size() + 1;
				offset += line.size() + 1;
				continue;
			}

			ele.clear();
			getElementsFromLine(line, seps, ele);
			if (ele.size() < 1)
			{
				length += line.size() + 1;
				offset += line.size() + 1;
				continue;
			}
			eles.clear();
			getElementsFromLine(ele[0], sep, eles);
			
			i_accss = eles.size()>1?eles.size()-2:0;
			acc = eles[i_accss];
			it = accToidx.find(acc);
			
			if (it == accToidx.end())
			{	
				TaxIDs.push_back(-1);
				accToidx[acc] = idx++;
			}
			seqData s;
			s.Name = file;
			s.Accss = acc;
			s.offset = offset;
			s.length = 0;

			if (!seqs.empty() && fileContainsSequence) {
				seqs.back().length = offset - seqs.back().offset;
			}
			seqs.push_back(s);

			offset += line.size() + 1;
			length = line.size() + 1;
			fileContainsSequence = true;
            foundSequencesCount++;
		}

        cerr << " Sequences found: " << foundSequencesCount << endl;
        filesCounter++;

		if (fileContainsSequence && !seqs.empty()) {
			seqs.back().length = offset - seqs.back().offset;
		}

        fclose(fd);
	}
	fclose(meta_f);
    cerr << "Loading accession number of all files done ("<< accToidx.size() << "sequences in " << filesCounter << "files)" << endl;

	string on_line;
	sep.push_back(' ');
	sep.push_back('\t');
	std::map<int, int> 		oldTonew;
	std::map<int, int>::iterator	it_on;

    std::cerr << "Loading merged Tax ID... " << endl;
	while (getLineFromFile(oldTx,on_line))
	{
		ele.clear();
		getElementsFromLine(on_line, sep, ele);
		it_on = oldTonew.find(atoi(ele[0].c_str()));
		if (it_on == oldTonew.end())
		{
			oldTonew[atoi(ele[0].c_str())] = atoi(ele[1].c_str());
		}
	}	
	fclose(oldTx);
    std::cerr << "Loading merged Tax ID done" << std::endl;

	string pair;
	int taxID, new_taxID;
        vector<char> sepg;
        sepg.push_back(' ');
        sepg.push_back('\t');
	uint32_t cpt = 0, cpt_u = 0;
        cerr << "Retrieving taxonomy ID for each file... " << std::endl;
        size_t taxidTofind = TaxIDs.size(), taxidFound = 0;
        size_t counter = 0;

        const size_t LINES_STEP = 10000000;

	while (getLineFromFile(accToTx, pair) && taxidFound < taxidTofind)
        {
        counter++;
        if (counter % LINES_STEP == 0) {
            cerr << "Tax IDs processed: " << counter << endl;
        }

                ele.clear();
                getElementsFromLine(pair, sepg, ele);
                acc = ele[0];
		taxID = atoi(ele[2].c_str());
                it = accToidx.find(acc);
		if (it != accToidx.end())
		{
			taxidFound++;
			new_taxID = taxID;
			it_on = oldTonew.find(taxID);
			if (it_on != oldTonew.end())
			{	new_taxID = it_on->second;	}
			TaxIDs[it->second] = new_taxID;
		}
        }
        
        ofstream rejected(strcat(argv[1], "_rejected"));
	for(size_t t = 0; t < seqs.size(); t++)
	{
		cout << seqs[t].Name << ":" << seqs[t].offset << ";" << seqs[t].length << "\t" ;
		it = accToidx.find(seqs[t].Accss);
		cout << seqs[t].Accss << "\t" << TaxIDs[it->second] << endl;
		if (TaxIDs[it->second]  == -1) {
			rejected << seqs[t].Name << ":" << seqs[t].offset << ";" << seqs[t].length << "\t" << seqs[t].Accss << endl;
			cpt_u++; }
		else
		{	cpt++;	 }
	}
	cerr << "done (" << cpt << " files were successfully mapped";
	if (cpt_u > 0)
	{	cerr <<  ", and "<< cpt_u << " unidentified";	}
	cerr << ")." << endl;
	return 0;
}

