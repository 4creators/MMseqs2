/*
 * gff2ffindex
 * written by Milot Mirdita <milot@mirdita.de>
 */

#define _GNU_SOURCE 1
#define _LARGEFILE64_SOURCE 1
#define _FILE_OFFSET_BITS 64

#include <string>
#include <fstream>
#include <sstream>

#include "DBReader.h"
#include "DBWriter.h"
#include "Debug.h"
#include "Parameters.h"
#include "Util.h"

int gff2ffindex(int argn, const char **argv) {
    std::string usage("Converts a gff file and the matching ffindex database into a ffindex.\n");
    usage.append("USAGE: <gff3>  <ffindexInDB> <ffindexOutDB>\n");
    usage.append("\nDesigned and implemented by Martin Steinegger <martin.steinegger@mpibpc.mpg.de>.\n");

    Parameters par;
    par.parseParameters(argn, argv, usage, par.gff2ffindex, 3);
    Debug::setDebugLevel(par.verbosity);

    std::string ffindex_filename = par.db2;
    std::string ffindex_index_filename = par.db2Index;

    std::string ffindex_filename_hdr(ffindex_filename);
    ffindex_filename_hdr.append("_h");

    std::string ffindex_index_filename_hdr(ffindex_filename);
    ffindex_index_filename_hdr.append("_h.index");

    DBReader<std::string> ffindex_reader(ffindex_filename.c_str(), ffindex_index_filename.c_str());
    DBReader<std::string> ffindex_hdr_reader(ffindex_filename_hdr.c_str(), ffindex_index_filename_hdr.c_str());

    ffindex_reader.open(DBReader<std::string>::NOSORT);
    ffindex_hdr_reader.open(DBReader<std::string>::NOSORT);

    std::string data_filename = par.db3;
    std::string index_filename = par.db3Index;

    std::string data_filename_hdr(data_filename);
    data_filename_hdr.append("_h");

    std::string index_filename_hdr(data_filename);
    index_filename_hdr.append("_h.index");

    DBWriter out_writer(data_filename.c_str(), index_filename.c_str());
    DBWriter out_hdr_writer(data_filename_hdr.c_str(), index_filename_hdr.c_str());

    out_writer.open();
    out_hdr_writer.open();

    bool shouldCompareType = par.gffType.length() > 0;

    size_t entries_num = 0;

    std::ifstream  file_in(par.db1);
    std::string    gff_line;
    while(std::getline(file_in, gff_line)) {
        entries_num++;

        // line is a comment
        if(gff_line.find_first_of("#", 0, 1) != std::string::npos) {
            continue;
        }

        std::vector<std::string> fields = Util::split(gff_line, "\t");

        // gff has always 9 fields
        if(fields.size() != 9) {
            Debug(Debug::WARNING) << "Invalid GFF format in line " << entries_num << "!";
            continue;
        }

        std::string name(fields[0]);

        std::string type(fields[2]);

        if(shouldCompareType && type.compare(par.gffType) != 0) {
            continue;
        }

        char* rest;
        size_t start = strtoull(fields[3].c_str(), &rest, 10);
        if ((rest != fields[3].c_str() && *rest != '\0') || errno == ERANGE) {
            Debug(Debug::WARNING) << "Invalid start position format in line " << entries_num << "!\n";
            continue;
        }
        size_t end = strtoull(fields[4].c_str(), &rest, 10);
        if ((rest != fields[4].c_str() && *rest != '\0') || errno == ERANGE) {
            Debug(Debug::WARNING) << "Invalid end position format in line " << entries_num << "!\n";
            continue;
        }

        if (start == end) {
            Debug(Debug::WARNING) << "Invalid sequence length in line " << entries_num << "!\n";
            continue;
        }

        size_t length = end - start;

        size_t headerId = ffindex_hdr_reader.getId(name);

        if(headerId == UINT_MAX) {
            Debug(Debug::ERROR) << "GFF entry not found in fasta ffindex: " << name << "!\n";
            return EXIT_FAILURE;
        }

        char* header = ffindex_hdr_reader.getData(headerId);
        size_t headerLength = ffindex_hdr_reader.getSeqLens(headerId);

        char* body = ffindex_reader.getDataByDBKey(name);

        if(!header || !body) {
            Debug(Debug::ERROR) << "GFF entry not found in fasta ffindex: " << name << "!\n";
            return EXIT_FAILURE;
        }

        std::string id;
        if (par.useHeader) {
            id = Util::parseFastaHeader(body);
        } else {
            id = SSTR(par.identifierOffset + entries_num);
        }

        // header
        char buffer[headerLength + 128];
        if(shouldCompareType) {
            snprintf(buffer, headerLength + 128, "%s %s:%zu-%zu\n", header, type.c_str(), start, end);
        } else {
            snprintf(buffer, headerLength + 128, "%s %zu-%zu\n", header, start, end);
        }

        // hack: header contains a new line, lets just overwrite the new line with a space
        buffer[headerLength - 2] = ' ';
        out_hdr_writer.write(buffer, strlen(buffer), id.c_str());

        // sequence
        char bodyBuffer[length + 1];
        strncpy(bodyBuffer, body + start, length);
        bodyBuffer[length] = '\n';
        out_writer.write(bodyBuffer, length + 1, id.c_str());
    }
    file_in.close();

    out_hdr_writer.close();
    out_writer.close();

    return EXIT_SUCCESS;
}


