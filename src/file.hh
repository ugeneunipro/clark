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

#ifndef FILE_HH
#define FILE_HH

#include<string>
#include<stdio.h>
#include<stdint.h>
#include<vector>
#include "./dataType.hh"

#ifdef _WINDOWS
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
ssize_t getline(char **lineptr, size_t *n, FILE *stream);
#define strtok_safe strtok_s
#else 
#define strtok_safe strtok_r
#endif 


void getElementsFromLine(const std::string& line, size_t len, size_t _maxElement, std::vector< std::string >& _elements);

void getElementsFromLine(const std::string& line, const std::vector<char>& _seps, std::vector< std::string >& _elements);

void getElementsFromLine(const std::string& line, size_t _maxElement, std::vector< std::string >& _elements);

bool getLineFromFile(FILE*& _fileStream, std::string& _line);

bool getFirstElementInLineFromFile(FILE*& _fileStream, std::string& _line);

bool getFirstAndSecondElementInLine(FILE*& _fileStream, std::string& _line, ITYPE& _freq);

bool getFirstAndSecondElementInLine(FILE*& _fileStream, uint64_t& _kIndex, ITYPE& _index);

void mergePairedFiles(const char* _file1, const char* _file2, const char* _objFile);

void deleteFile(const char* _filename);

bool validFile(const char* _file);

void splitTargetPath(const std::string& targetPath, std::string& _filePath, size_t &_offset, size_t &_length);

#endif //FILE_HH
