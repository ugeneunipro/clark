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

#include <cstdlib>
#include <string>
#include <vector>
#include <string.h>
#include <fstream>

using namespace std;

#include "./file.hh"

void getElementsFromLine(const char* line, const size_t len, const int _maxElement, std::vector< std::string >& _elements)
{
	size_t t = 0; 
	size_t cpt = 0;
	_elements.resize(0);
	while (t < len && cpt < _maxElement)
	{
		while ( t < len  && (line[t] == ' ' || line[t] == '\t' || line[t] == '\n' || line[t] == '\r'))
		{       t++;
		}
		string v = "";
		while ( t < len && line[t] != ' '  && line[t] != '\t' && line[t] != '\n' && line[t] != '\r')
		{
			v.push_back(line[t]);
			t++;
		}
		if (v != "")
		{
			_elements.push_back(v);
			cpt++;
		}
	}
	return;
}

void getElementsFromLine(const std::string& line, const size_t& _maxElement, std::vector< std::string >& _elements)
{
	size_t t = 0, len = line.size();
	size_t cpt = 0;
	_elements.resize(0);
	while (t < len && cpt < _maxElement)
	{
		while (t <  len && (line[t] == ' ' || line[t] == ',' || line[t] == '\n' || line[t] == '\t' || line[t] == '\r'))
		{       t++;
		}
		string v = "";
		while (t <  len && line[t] != ' '  && line[t] != ',' && line[t] != '\n' && line[t] != '\t' && line[t] != '\r')
		{
			v.push_back(line[t]);
			t++;
		}
		if (v != "")
		{	
			_elements.push_back(v);
			cpt++;
		}
	}
	return;
}

void getElementsFromLine(const std::string& line, const vector<char>& _seps, std::vector< std::string >& _elements)
{
	size_t len = line.size();
	_elements.resize(0);
	const char* delims = &_seps.front();
	
	char* buf = new char[len + 1];
	line.copy(buf, len);
	buf[len] = '\0';
	char* t = NULL;

	char* next = strtok_safe(buf, delims, &t);
	while (next != NULL) {
		_elements.push_back(string(next));
		next = strtok_safe(NULL, delims, &t);
	}
	delete[] buf;
	return;
}

bool getLineFromFile(FILE*& _fileStream, string& _line)
{
	_line.clear();
	char *line = NULL;
	size_t len = 0;
	ssize_t read = getline(&line, &len, _fileStream);
	if (read != -1)
	{
		if (line[read - 1] == '\n' || line[read - 1] == '\r') {
			line[read - 1] = '\0';
		}
		_line.append(line);
	}
	free(line);
	 
	return _line.size() != 0;
}

bool getFirstElementInLineFromFile(FILE*& _fileStream, string& _line)
{
	char *line = NULL;
	size_t len = 0;
	if (getline(&line, &len, _fileStream) != -1)
	{
		vector<string> ele;
		getElementsFromLine(line, len, 1, ele);
		_line = ele[0];
		free(line);
		line = NULL;
		return true;
	}
	else
	{
		_line = "";
		return false;
	}
}

bool getFirstAndSecondElementInLine(FILE*& _fileStream, uint64_t& _kIndex, ITYPE& _index)
{
	char *line = NULL;
	size_t len = 0;
	if (getline(&line, &len, _fileStream) != -1)
	{
		// Take the first element and put it into _kIndex: type IKMER
		// Take the second element and put it into _index: type ITYPE
		vector<string> ele;
		getElementsFromLine(line, len, 2, ele);

		_kIndex = atoll(ele[0].c_str());
		_index = atol(ele[1].c_str());
		free(line);
                line = NULL;
		return true;
	}
	return false;
}

bool getFirstAndSecondElementInLine(FILE*& _fileStream, std::string& _line, ITYPE& _freq)
{
	char *line = NULL;
	size_t len = 0;
	if (getline(&line, &len, _fileStream) != -1)
	{
		// Take first element and put it into _line
		// Take second element and put it into _freq
		vector<string> ele;
		getElementsFromLine(line, len, 2, ele);
		_line = ele[0];
		_freq = atoi(ele[1].c_str());
		free(line);
                line = NULL;
		return true;
	}
	return false;
}


void mergePairedFiles(const char* _file1, const char* _file2, const char* _objFile)
{
        string line1, line2 = "";
        vector<string> ele1;
        vector<string> ele2;
        vector<char> sep;
        sep.push_back(' ');
        sep.push_back('/');
        sep.push_back('\t');
        FILE * fd1 = fopen(_file1, "r");
        FILE * fd2 = fopen(_file2, "r");
        getLineFromFile(fd1, line1);
        getLineFromFile(fd2, line2);
        if (line1[0] != line2[0])
        {
                perror("Error: the files have different format!");
                exit(1);
        }
        char delim = line1[0];
        bool isFastq = delim == '@';
	bool isFasta = delim == '>';
	if (!isFastq && !isFasta)
        {
                perror("Error: paired-end reads must be FASTQ files!");
                exit(1);
        }
        sep.push_back(delim);
        rewind(fd1);
        rewind(fd2);
        ofstream fout(_objFile, std::ios::binary);
        while(getLineFromFile(fd1, line1) && getLineFromFile(fd2, line2))
        {
                if (line1[0] == delim && line2[0] == delim)
                {
                        ele1.clear();
                        ele2.clear();
                        getElementsFromLine(line1, sep, ele1);
                        getElementsFromLine(line2, sep, ele2);
                        if (ele1[0] != ele2[0])
                        {
                                perror("Error: read id does not match between files!");
                                exit(1);
                        }
                        fout << ">" << ele1[0] << endl;
                        if (getLineFromFile(fd1, line1) && getLineFromFile(fd2, line2))
                        {
                                // Add "NNNN" to concatenate sequences, and separate content of each sequence
                                fout << line1 << "NNNN" << line2 << endl;
                                if (isFastq)
				{
					if (getLineFromFile(fd1, line1) && getLineFromFile(fd2, line2))
                                	{
                                	        if (getLineFromFile(fd1, line1) && getLineFromFile(fd2, line2))
                                	        {       continue;       }
                                	}
				}
                        }
                        else
                        {
                                perror("Error: Found read without sequence");
                                exit(1);
                        }
                        continue;
                }
        }
        fclose(fd1);
        fclose(fd2);
        fout.close();
}

void deleteFile(const char* _filename)
{
        if (_filename != NULL)
                remove(_filename);
}

bool validFile(const char* _file)
{
        FILE * fd = fopen(_file, "r");
        if (fd == NULL)
        {       return false;   }
        fclose(fd);
        return true;
}

void splitTargetPath(const string& targetPath, string& _filePath, size_t& _offset, size_t& _length) {
    _filePath = targetPath;
    _offset = 0;
    _length = 0;

    size_t separatorIndex = targetPath.find_last_of(':');
    if (string::npos == separatorIndex) {
        return;
    }

    _filePath = targetPath.substr(0, separatorIndex);
    string positionData = targetPath.substr(separatorIndex + 1);

    separatorIndex = positionData.find_last_of(';');
    if (string::npos == separatorIndex) {
        return;
    }

    _offset = atoi(positionData.substr(0, separatorIndex).c_str());
    _length = atoi(positionData.substr(separatorIndex + 1).c_str());
}
