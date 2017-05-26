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

#include "sperf_instr.h"

using namespace std;

std::string intToString(int x)
{
    std::stringstream ss;
    ss << x;
    return ss.str();
}
int stringToInt(std::string x)
{
    int n;
    std::stringstream ss(x);
    ss >> n;
    return n;
}

void Instrumentation::read_config_file(string file_name)
{
    abre.open(file_name+".conf");
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
        for(auto ext: extensions)
            if(dir.find(ext)!=string::npos) /// AJEITAR
                files.push_back(dir);
    }
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
    for(auto crs: commentRegions)
        if(p>crs.li && p<crs.lf)
            return true;
    return false;
}

void Instrumentation::instrument()
{
    cout << BLUE "[Sperf]" RESET " Parsing the files..." << endl;
    for(auto file: files)
    {
        cout << "file " << file << endl;
        abre.open( (dpath+file).c_str() );
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
        p= txt.find("main");
        while(p != string::npos)
        {
            if(!isInsidComment(p, commentRegions))
            {
                while(txt[p] != '{')
                    p++;
                txt= txt.substr(0, p+1)+"\nsetconfig();\n"+txt.substr(p+1, txt.size());
                break;
            }
            p= txt.find("main", p+4);
        }
        p= txt.find("#pragma omp parallel");
        while(p != string::npos)
        {
            commentRegions.clear();
            FindEnclosures(txt, "//", "\n", commentRegions);
            FindEnclosures(txt, "/*", "*/", commentRegions);
            if(!isInsidComment(p, commentRegions))
            {
                haveOMP= true;
                string id= intToString(id_region);
                txt= txt.substr(0, p)+"sperf_start("+id+");\n"+txt.substr(p, txt.size());

                commentRegions.clear();
                FindEnclosures(txt, "//", "\n", commentRegions);
                FindEnclosures(txt, "/*", "*/", commentRegions);

                unsigned long long int stp= txt.find("\n", p+id.size()+16)+1;
                int cont= 0;
                bool first= false;
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
                txt= txt.substr(0, stp)+"\nsperf_stop("+id+");\n"+txt.substr(stp, txt.size());
                //cout << txt << endl;
                //cout << BLUE "[Sperf]" RESET "  marks on lines " << p << " " << stp << endl;
                p= txt.find("#pragma omp parallel", p+16+id.size());
                id_region++;
            }
            else
                p= txt.find("#pragma omp parallel", p+1);
        }
        if(haveOMP)
        {
            txt= "#include \"sperf_instr.h\"\n"+txt;
            ofstream salva((dpath+"/instr/"+file).c_str());
            salva << txt;
            salva.close();
        }
        abre.close();
    }
}
