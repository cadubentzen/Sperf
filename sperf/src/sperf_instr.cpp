/*
    Copyright (C) 2017 Márcio Jales, Vitor Ramos

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
*/

#include "../include/sperf_instr.h"
#include <stdlib.h>

typedef unsigned int uint;

using namespace std;


void Instrumentation::read_argments(char* argv[], int argc)
{
    for(int i=2; i<argc; i++)
    {
        string argm= argv[i];
        if(argm == "-p")
        {
            if(i+1<argc)
                dpath= string(argv[i+1]);
            else
                throw "Erro argumentos";
        }
        else if(argm == "-e")
        {
            if(i+1<argc)
            {
                argm= argv[i+1];
                stringstream ss(argm);
                while(getline(ss, argm, ','))
                {
                    extensions.push_back(argm);
                    cout << argm << " ";
                }
                cout << endl;
            }
            else
                throw "Erro argumentos";
        }
    }
    if(dpath.empty())
        throw "Erro precisa especificar os argumentos";
    else if(extensions.empty())
    {
        cout << BLUE "[Sperf]" RESET " Using default extensions .cpp .h" << endl;
        extensions.push_back(".cpp");
        extensions.push_back(".c");
        extensions.push_back(".h");
    }
    if(dpath.back() != '/')
        dpath+="/";
    if(dpath == "/")
    {
        dpath= string(argv[0]);
        dpath= dpath.substr(0,dpath.find_last_of("/")+1);
    }
}

void Instrumentation::read_config_file(string file_name)
{
    abre.open(string(file_name+".conf").c_str());
    if(!abre)
        throw "cant open the file "+file_name+".conf";
    while(!abre.eof())
    {
        getline(abre, buffer);
        if(buffer.find("#")!=string::npos)
            buffer= buffer.substr(0, buffer.find("#"));
        if(!buffer.empty())
        {
            //if(buffer == "DPATH")
            if(buffer.substr(0,6) == "DPATH=")
            {
                cout << BLUE "[Sperf]" RESET " Retriving the application's source code path" << endl;
                dpath= buffer.substr(6,buffer.size());
                if(dpath[dpath.size()-1] != '/')
                    dpath+="/";
            }
            else if(buffer.substr(0,11) == "EXTENSIONS=")
            {
                cout << BLUE "[Sperf]" RESET " Retriving the extensions's" << endl;
                buffer=buffer.substr(11,buffer.size());
                stringstream ss(buffer);
                while(getline(ss, buffer, ','))
                {
                    extensions.push_back(buffer);
                    cout << buffer << " ";
                }
                cout << endl;
            }
        }
    }
    abre.close();
}
void Instrumentation::getFileNames()
{
    cout << BLUE "[Sperf]" RESET " Retriving file names" << endl;
    dp = opendir(dpath.c_str());
    if(dp == NULL)
        throw "cant open the directory "+dpath;
    while((dptr= readdir(dp)) != NULL)
    {
        string dir= dptr->d_name;
        for(uint i=0; i<extensions.size(); i++)
        //for(auto ext: extensions)
            if(dir.find(extensions[i])!=string::npos) /// AJEITAR
            {
                cout << "\t" << dir << endl;
                files.push_back(dir);
                break;
            }
    }
}

uint64_t Instrumentation::findOMPdirectives(string& txt, uint64_t s)
{
    string tofind= "#pragmaompparallel";
    uint64_t p= string::npos;
    for(uint64_t l=s; l<txt.size(); l++)
    {
        uint64_t r=l, cont_= 0;
        while(r<txt.size() && cont_ < tofind.size() && txt[r] == tofind[cont_++])
        {
            r++;
            while(r<txt.size() && txt[r] == ' ')
                r++;
        }
        if(cont_ == tofind.size())
        {
            p= l;
            break;
        }
    }
    return p;
}

void Instrumentation::FindEnclosures(string& txt, string e1, string e2, vector<commentRegion> &commentRegions)
{
    commentRegion cR;
    unsigned long long int p= txt.find(e1);
    while(p!=string::npos)
    {
        //txt.erase(txt.begin()+p, txt.begin()+txt.find("\n",p+1));
        cR.li= p;
        cR.lf= txt.find(e2,p+e2.size());
        commentRegions.push_back(cR);
        p= txt.find(e1, cR.lf);
    }
}

bool Instrumentation::isInsidComment(unsigned long long int p, std::vector<commentRegion> &commentRegions)
{
    for(uint i=0; i<commentRegions.size(); i++)
    //for(auto crs: commentRegions)
        if(p>commentRegions[i].li && p<commentRegions[i].lf)
            return true;
    return false;
}

void Instrumentation::instrument()
{
    if(opendir(string(dpath+"instr/").c_str()) == NULL)
    {
        cout << BLUE "[Sperf]" RESET " Creating folder /inst" << endl;
        mkdir(string(dpath+"instr/").c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
        if(opendir(string(dpath+"instr/").c_str()) == NULL)
            throw  "Failed to create the result folder: \n";
    }
    cout << BLUE "[Sperf]" RESET " Parsing the files..." << endl;
    for(uint i=0; i<files.size(); i++)
    //for(auto file: files)
    {
        string file= files[i];
        cout << file << endl;
        abre.open( (dpath+file).c_str() );
        if(!abre)
        {
            string err= "Cant opent the file"+dpath+file;
            throw err.c_str();
        }
        string txt;
        while(!abre.eof())
        {
            getline(abre, buffer);
            txt=txt+buffer+"\n";
        }
        vector<commentRegion> commentRegions;
        unsigned long long int p;
        bool haveOMP= false;
        int id_region= 0;

        FindEnclosures(txt, "//", "\n", commentRegions);
        FindEnclosures(txt, "/*", "*/", commentRegions);
//        p= txt.find("main");
//        while(p != string::npos)
//        {
//            if(!isInsidComment(p, commentRegions))
//            {
//                while(txt[p] != '{')
//                    p++;
//                txt= txt.substr(0, p+1)+"\nsetconfig();\n"+txt.substr(p+1, txt.size());
//                break;
//            }
//            p= txt.find("main", p+4);
//        }
        //p= txt.find("#pragma omp parallel");
        p= findOMPdirectives(txt, 0);
        while(p != string::npos)
        {
            commentRegions.clear();
            FindEnclosures(txt, "//", "\n", commentRegions);
            FindEnclosures(txt, "/*", "*/", commentRegions);
            if(!isInsidComment(p, commentRegions))
            {
                haveOMP= true;
                string id= to_string(id_region);
                txt= txt.substr(0, p)+"sperf_start("+id+");\n"+txt.substr(p, txt.size());

                commentRegions.clear();
                FindEnclosures(txt, "//", "\n", commentRegions);
                FindEnclosures(txt, "/*", "*/", commentRegions);

                unsigned long long int stp= txt.find("\n", p+id.size()+16)+1;
//                int cont= 0;
//                bool first= false;
/*
                while(cont != 0 || !first)
                {
                    if(txt[stp] == '{' && !isInsidComment(stp, commentRegions))
                    {
                        first= true;
                        cont++;
                    }
                    if(txt[stp] == '}' && !isInsidComment(stp, commentRegions))
                        cont--;
                    stp++;
                    if(stp >= txt.size())
                    {
                        throw  "ERRO\n";
                    }
                }
*/
				do
                {
                    while(isInsidComment(stp, commentRegions))
                        stp++;
                    if(txt[stp]=='(')
                    {
                        stp++;
                        int contp= 1;
                        do
                        {
                            if(txt[stp]=='(')
                                contp++;
                            if(txt[stp]==')')
                                contp--;
                            stp++;
                        }while(stp<txt.size() && contp>0);
                        continue;
                    }
                    if(txt[stp] == '{')
                    {
                        stp++;
                        int contp= 1;
                        do
                        {
                            if(txt[stp]=='{')
                                contp++;
                            if(txt[stp]=='}')
                                contp--;
                            stp++;
                        }while(stp<txt.size() && contp>0);
                        break;
                    }
                    if(txt[stp]==';')
                    {
                        stp++;
                        break;
                    }
                    stp++;
                }while(stp<txt.size());

                txt= txt.substr(0, stp)+"\nsperf_stop("+id+");\n"+txt.substr(stp, txt.size());
                //cout << txt << endl;
                //cout << BLUE "[Sperf]" RESET "  marks on lines " << p << " " << stp << endl;
                //p= txt.find("#pragma omp parallel", p+16+id.size());
                p= findOMPdirectives(txt, p+16+id.size());
                id_region++;
            }
            else
                p= findOMPdirectives(txt, p+1);
                //p= txt.find("#pragma omp parallel", p+1);
        }
        if(haveOMP)
        {
            cout <<  BLUE "[Sperf]" RESET " File instrumented" << endl;
            ifstream sperfops("../include/sperfops.h");
            if(!sperfops)
                throw "Cant open sperfops.h";
            string buff, sperfops_h;
            while(getline(sperfops, buff))
                sperfops_h+=buff+"\n";
            txt= sperfops_h+txt;
            ofstream salva((dpath+"/instr/"+file).c_str());
            salva << txt;
            salva.close();
        }
        abre.close();
    }
}
