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
            if(dir.find(ext)!=string::npos)
                files.push_back(dir);
    }
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
        commentRegion cR;
        vector<commentRegion> commentRegions;
        unsigned long long int p= txt.find("//");
        while(p!=string::npos)
        {
            //txt.erase(txt.begin()+p, txt.begin()+txt.find("\n",p+1));
            cR.li= p;
            cR.lf= txt.find("\n",p+1);
            commentRegions.push_back(cR);
            p= txt.find("//", cR.lf);
        }
        p= txt.find("/*");
        while(p!=string::npos)
        {
            //txt.erase(txt.begin()+p, txt.begin()+txt.find("*/",p+1)+2);
            cR.li= p;
            cR.lf= txt.find("*/",p+1)+2;
            commentRegions.push_back(cR);
            p= txt.find("/*", cR.lf);
        }
        bool haveOMP= false;
        int id_region= 0;
        p= txt.find("#pragma omp parallel");
        while(p != string::npos)
        {
            bool insidComment= false;
            for(auto crs: commentRegions)
            {
                if(p>crs.li && p<crs.lf)
                {
                    insidComment= true;
                    break;
                }
            }
            if(!insidComment)
            {
                haveOMP= true;
                string id= intToString(id_region);
                txt= txt.substr(0, p)+"sperf_start("+id+");\n"+txt.substr(p, txt.size());
                int stp= txt.find("\n", p+id.size()+16)+1;
                bool first= false;
                int cont= 0;
                while(cont != 0 || !first)
                {
                    if(txt[stp] == '{')
                    {
                        first= true;
                        cont++;
                    }
                    if(txt[stp] == '}')
                        cont--;
                    stp++;
                }
                txt= txt.substr(0, stp)+"\nsperf_stop("+id+");\n"+txt.substr(stp, txt.size());
                cout << txt << endl;
                //cout << BLUE "[Sperf]" RESET "  marks on lines " << p << " " << stp << endl;
            }
            p= txt.find("#pragma omp parallel", p+16+intToString(id_region).size());
            id_region++;
        }
        if(haveOMP)
            txt= "#include \"sperf_instr.h\"\n"+txt;
        abre.close();
        ofstream salva((dpath+"/instr/"+file).c_str());
        salva << txt;
        salva.close();
    }
}
