/*
 
**********************************************************************
 Copyright 2009, 2010 by Dan (Dacust), Henrik Jessen, Marco Certelli
**********************************************************************

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

**********************************************************************

Newest version at: http://www.dacust.com/inlandwaters/mapcal/

Update 01/27/2010:
1) PP=central meridian for Transverse Mercator Projections
2) added key OST/1
3) added CPH/0.0 if not 180.0
4) keep shorter rows (chart name in a single line)

Update 01/09/2010:
1) Show mc2bsbh version in "Usage" printouts.
 
Update 11/18/2009:
1) corrected DTM calculation from DS*60 to DS*3600

Update 11/13/2009:
1) corrected an old bug (try mc2bsbh -e) and a bug introduced in 05
2) added a memory check
3) added a new switch "-l" to list all the maps present in CHARTCAL.DIR
4) added the BSB parameter CPH/180.0 if the map crosses 180° longitude
5) ignore CHARTCAL.DIR lines beginning with ;
6) improved printout of Usage:
7) changed switch -h in -o (since -h is normally for help)

Update 11/13/2009:
1) header length now unlimited
2) changes to compile with gcc 4.3.2
3) some internal code changes for consistancy and maintainability

Update 11/10/2009:
1) corrected a bug with header longer than 40 lines - now 250
2) corrected a bug with empty parameters in CHARTCAL.DIR
3) added a preprocessor of the MapCal comment field
4) a new switch -s MAPNAME to generate a single header from multiple CHARTCAL.DIR
5) removed obsolete switch -ced (see point 3)

Update 11/09/2009:
1) now it compiles on BCC32 (check for g++)
2) scales > 1000000 is not translated in SC=1e6
3) if longitude > 180, it writes long-360
4) rouded DU value (dot per inch is an integer)
5) corrected bug: DTM is DS * 60 (not just DS)
6) lat and long are written with a right number of decimals (>=6).

Update 11/08/2009:
New version 0.0.2 - Rewritten by a real c++ programmer - Henrik Jessen. Totally new code. Less source and smaller executable. I will have to look at it to figure out how it works. It also now supports multiple calibrations in a single CHARTCAL.DIR file. None of the switches have changed from my last version. Please test and let me know how it works. The old source is still available below.

***************************************************************************
*/

// to compile:  g++ -Wall -s -O2 mc2bsbh.cpp -o mc2bsbh

#define VERSION "beta09"

#include <iostream>
#include <cmath>
#include <fstream>
#include <cstdlib>
#include <cstdio>
#include <vector>

using namespace std;

// The command line options in a combined struct for easy access
struct command_line_info
{
    bool debug_on;
    bool list;
    string sw_single;
    string sw_ext;
    string sw_out_name;
    string in_filename;
};

// Store the input lines in a searchable buffer
class input_buffer
{
private:
    vector<string> lines;
public:
    void reset()                { lines.clear(); }
    unsigned int count() const  { return lines.size(); }
    bool empty() const          { return lines.empty(); }
    input_buffer()              { reset(); }    // Constructor

    void add_line(const string &new_line);
    bool append_line(const string &append_line);
    const string line(unsigned int line_number) const;
    const string field(const string &field_name) const;
    double field_double(const string &field_name, unsigned int index = 0) const;
    long int field_long(const string &field_name, unsigned int index = 0) const;
    string field_string(const string &field_name, unsigned int index) const;
    
    static const long int NAN_L;
    static const double NAN_D;
};

// We assume that this value will never appear in a valid MapCal file.
const long int input_buffer::NAN_L = -0x7FFFFFFF;
const double input_buffer::NAN_D = input_buffer::NAN_L;

const string extract_field(const string &in_string, unsigned int field_number);

// Add a line to the buffer
void input_buffer::add_line(const string &new_line)
{
    lines.push_back(new_line);
}

// Append a line to last line in the buffer
bool input_buffer::append_line(const string &append_line)
{
    if (empty()) return false;
    
    lines[count()-1] += "\n" + append_line;
    return true;
}

// Get the line from the buffer with the given line number
const string input_buffer::line(unsigned int line_number) const
{
    if (line_number < count())
        return lines[line_number];
    else
        return "";
}

// Search and return a field from the given field name from the buffer 
const string input_buffer::field(const string &field_name) const
{
    unsigned int u = 0;
    unsigned int len = field_name.length();
    
    string result = "";
    while (u < count())
    {
        if (line(u).length() > (len+1) &&
        	line(u).compare(0, len, field_name) == 0 &&
            line(u).at(len) == '=')
        {
            result = line(u).substr(len+1);
            return result;
        }
        u++;
    }
    return "";
}

// Search field in buffer and convert to double (return NAN_D if undefined).
double input_buffer::field_double(const string &field_name, unsigned int index) const
{
    string info = extract_field(field(field_name), index);
    if (info.empty())
        return NAN_D;
    else
        return strtod(info.c_str(), 0);
}

// Search field in buffer and convert to double (return NAN_D if undefined).
string input_buffer::field_string(const string &field_name, unsigned int index) const
{
    string info = extract_field(field(field_name), index);
    return info.c_str();
}

// Search field in buffer and convert to long (return NAN_L if undefined) 
long int input_buffer::field_long(const string &field_name, unsigned int index) const
{
    return (long int) field_double(field_name, index);
}

// Extract field from a string with comma separated substrings 
const string extract_field(const string &in_string, unsigned int field_number)
{
    unsigned int cp = 0;
    
    while(field_number > 0 && cp != string::npos)
    {
        cp = in_string.find_first_of(',', cp);
        if (cp != string::npos) cp++;
        field_number--;
    }
    
    if (in_string.empty() || cp == string::npos)
    {
        return "";
    }
    else
    {
        unsigned int ep = in_string.find_first_of(',', cp);
        if (ep == string::npos)
        {
            ep = in_string.length();
        }
        
        return in_string.substr(cp, ep-cp);
    }
}

// int to string convert
string itoa(int value)
{
    char cbuf[15];
    snprintf(cbuf, sizeof(cbuf), "%d", value);
    cbuf[sizeof(cbuf)-1] = 0;
    return string(cbuf);
}

// Remove trailing whitespaces
string &trim_trailing(string &s)
{
    unsigned int last  = s.find_last_not_of("\n\r\t ");
    if (last != string::npos)
    {
        s = s.substr(0, last+1);
    }
    return s;
}

// Remove leading and trailing whitespaces
string &trim(string &s)
{
    unsigned int first = s.find_first_not_of("\n\r\t ");
    if (first == string::npos)
    {
        s = "";
    }
    else
    {
        int last  = s.find_last_not_of("\n\r\t ");
        s = s.substr(first, last+1-first);
    }
    return s;
}

// Convert a section of a MapCal file. The strings are stored in an input_buffer
void convert_section(input_buffer &buf, const command_line_info &opt)
{
    string mc_chart_name;
    string st;

    // Line 0 is always the chart title enclosed in []
    mc_chart_name = buf.line(0).substr(1, buf.line(0).length()-2);
    unsigned int find_end = mc_chart_name.find_last_of('.');
    if (find_end != string::npos)
    {
        mc_chart_name=mc_chart_name.substr(0,find_end);
    }

    if (opt.sw_out_name.empty())
    {
        if (opt.sw_ext.empty())
            st = mc_chart_name + ".hdr";
        else
            st = mc_chart_name + "." + opt.sw_ext;
    }
    else
    {
         st = opt.sw_out_name;
    }
  
    // Open file
    cout << "Create "  << st << endl;
    ofstream outFile ( st.c_str() );

    // Calculate derived values
    long int sc = buf.field_long("SC"); 
    double   dx = buf.field_double("DX"); 
    double   dy = buf.field_double("DY");
    long int du = 0;
    
    if (sc != 0   && sc != buf.NAN_L &&
        dx != 0.0 && dx != buf.NAN_D &&
        dy != 0.0 && dy != buf.NAN_D)
    {
        du = (long int) ((sc * 2.54 / (( dx + dy ) / 2.0 * 100.0)) + 0.5);
    }
    
    long int projection;
    projection = buf.field_long("PR");
    if ((projection==buf.NAN_L)||(projection>3)) projection = 0;
    string pr = extract_field("UNKNOWN,MERCATOR,TRANSVERSE MERCATOR,LAMBERT CONFORMAL CONIC", projection);
       
    long int i = buf.field_long("DU");
    if (i==buf.NAN_L) i = 0;
    string un = extract_field("UNKNOWN,METERS,FEET,FATHOMS", i);
    
    // Output the file
    outFile << "! Created by mc2bsbh " << VERSION << " - Use at your own risk!" << endl;
    
    // In this section the CR field is processed. The CR field is the CHARTCAL.DIR comment and
    // it is made of multiple lines first of which is CR=... and next starts with a space char.
    // Each line may be a comment (to be just copied in the BSB header) or a command.
    // Command starts with the keyword BSBHDR and may be of 2 types:
    //    1) commands for superseeding default BSB and KNP values:
    //		  - these commands are in the form BSBHDR KNP/SC=xxx,PP=yyy
    //    2) commands for adding a totally new line to the header
    //		  - these commands never use KNP/ or BSB/ parameter.
    
    st = buf.field("CR");

    if (!st.empty())
    {
        string cline,tline;
    
        input_buffer tmp;
    
        unsigned int brk = 0;
        int addn = 1, tfield;
        
        do
        {
            unsigned int nxt = st.find_first_of("\t\r\n",brk);
            if (nxt == string::npos) nxt = st.length() + 1;
            cline = st.substr(brk, nxt-brk);
            
            if (cline.substr(0,6) == "BSBHDR")
            {
                cline=cline.substr(6);
                
                while (cline.at(0) == ' ') cline=cline.substr(1);
                
                if (cline.substr(0,4)=="KNP/")
                {
                    tmp.add_line("KNP="+cline.substr(4));
					
                    tfield=0;
                    tline=tmp.field_string("KNP",tfield);
                    while (!tline.empty())
                    {
                        buf.add_line(tline);
                        tfield++;
                        tline=tmp.field_string("KNP",tfield);
                    }  
                }
                else if (cline.substr(0,4)=="BSB/")
                {
                    tmp.add_line("BSB="+cline.substr(4));
					
                    tfield=0;
                    tline=tmp.field_string("BSB",tfield);
                    while (!tline.empty())
                    {
                        buf.add_line(tline);
                        tfield++;
                        tline=tmp.field_string("BSB",tfield);
                    }
                }
                else
                {
                    buf.add_line("ADD"+itoa(addn)+"="+cline);
                    addn++;
                }
            }
            else
            {
                outFile << "! " << cline << endl;
            }
            
            brk=nxt+1;
        } while (brk <= st.length());
    }
   
    string pp,pi,sp,sk,ta,sd;
    
    if (buf.field_double("PP")==buf.NAN_D) pp="UNKNOWN"; else pp=buf.field("PP");
    if (buf.field_double("PI")==buf.NAN_D) pi="UNKNOWN"; else pi=buf.field("PI");
    if (buf.field_double("SP")==buf.NAN_D) sp="UNKNOWN"; else sp=buf.field("SP");
    if (buf.field_double("SK")==buf.NAN_D) sk="0.0";     else sk=buf.field("SK");
    if (buf.field_double("TA")==buf.NAN_D) ta="90.0";    else ta=buf.field("TA");
    if (buf.field("SD").empty())           sd="UNKNOWN"; else sd=buf.field("SD");     
    
    outFile << "VER/2.0" << endl;
    
    outFile << "BSB/NA=" << buf.field("NA") << endl;
    outFile << "    NU=" << buf.field("NU") << ",RA=" << buf.field("WI") << "," << buf.field("HE") << ",DU=" << du << endl;

    outFile << "KNP/SC=" << sc << ",GD=" << buf.field("GD") << ",PR=" << pr;

    double lon0 = buf.field_double("LON0");
    if ((lon0!=buf.NAN_D)&&(projection==2)&&(pp=="UNKNOWN"))
         outFile << ",PP=" << lon0 << endl;
    else
         outFile << ",PP=" << pp << endl;

    outFile << "    PI=" << pi << ",SP=" << sp << ",SK=" << sk << ",TA=" << ta << endl;
    outFile << "    UN=" << un << ",SD=" << sd << endl;
    outFile << "    DX=" << dx << ",DY=" << dy << endl;

    unsigned int cnt;
    
    cnt=1;
    for(;;)
    {
        st = buf.field("ADD" + itoa(cnt));
        if (st.empty())
            break;

        outFile << st << endl;        
        
        cnt++;
    }
    
    outFile << "OST/1" << endl;
    
    long int refx, refy, maxlonx, minlonx;
    double lat, lon, maxlon=-181.0, minlon=181.0;
    
	outFile.precision(9);
    cnt=1;
    for(;;)
    {
        st = buf.field("C" + itoa(cnt));
        if (st.empty())
            break;
            
        refx = buf.field_long("C" + itoa(cnt),0);
        refy = buf.field_long("C" + itoa(cnt),1);        
        lat  = buf.field_double("C" + itoa(cnt),2);
        lon  = buf.field_double("C" + itoa(cnt),3);
        
        if (lon>180.0) lon=lon-360.0;
        if (lon>maxlon) { maxlon=lon; maxlonx=refx;}
        if (lon<minlon) { minlon=lon; minlonx=refx;}
        
        outFile << "REF/" << cnt << "," << refx << "," << refy << ","
                                        << lat << "," << lon << endl;
        cnt++;
    }
    
    if((maxlon*minlon)<0.0 && (maxlonx < minlonx))
    {
        outFile << "CPH/180.0" << endl;
    }
    else
    {
        outFile << "CPH/0.0" << endl;
    }
        
    cnt=1;
    for(;;)
    {
        st = buf.field("B" + itoa(cnt));
        if (st.empty())
            break;
            
        lat  = buf.field_double("B" + itoa(cnt),0);
        lon  = buf.field_double("B" + itoa(cnt),1);
        
        if (lon>180.0) lon=lon-360.0;
         
        outFile << "PLY/" << cnt << "," << lat << "," << lon << endl;        
        
        cnt++;
    }
    
    outFile << "DTM/" << buf.field_double("DS",0)*3600.0 << ","
            << buf.field_double("DS",1)*3600.0 << endl;

    outFile.close();
}

void ExitError(string error)
{
    cout << error << endl;
    exit(1);
}


// Main; read and split the file in sections
int main ( int argc, char *argv[] )
{
    command_line_info opt;//= {0};  //this does not work in Borland bcc32... 
    
    opt.debug_on=false;
    opt.list=false;
    opt.sw_single="";
    opt.sw_ext="";
    opt.sw_out_name="";
    opt.in_filename="";

    string incoming;
    int argcount;
    unsigned int find_end, nout;
    string in_switch, chart_name;
    
    // Scan command line arguments
    argcount=1;
    while (argcount < argc)
    {
        in_switch = argv[argcount];
        
        if (in_switch == "-d")         // -d (run in debug mode)
        {
            opt.debug_on=true;
        }
        else                           // -l (list of all charts)
        if (in_switch == "-l")
        {
            opt.list=true;
        }
        else                           // -s SingleChart (extract a single chart)
        if (in_switch == "-s" && argcount < argc-1)
        {
            argcount++;
            opt.sw_single = argv[argcount];
        }
        else                          // -e File extension (force file extension)
        if (in_switch == "-e" && argcount < argc-1)
        {
            argcount++;
            opt.sw_ext = argv[argcount];
        }
        else                          // -o Header file name (force header Name)
        if (in_switch == "-o" && argcount < argc-1)
        {
            argcount++;
            opt.sw_out_name = argv[argcount];
        }
        else
        if (in_switch.at(0) != '-')
        {
            opt.in_filename = argv[argcount];
        }

        argcount++;
    }

    if ( opt.in_filename.empty() )
    {
        cout<<endl;
        cout<<"mc2bsbh ("<<VERSION<<"): converts georeference format from MapCal to BSB header\n\n";
        cout<<"Usage: mc2bsbh [-d] [-s chartname] [-o outfile | -e extension] [-l] <infile>"<<endl<<endl;
        cout<<"       <infile>     : the output from MapCal - normally CHARTCAL.DIR"<<endl;
        cout<<"       -d           : this is debug mode. It prints out a bunch of garbage"<<endl;
        cout<<"       -s chartname : convert a single chart header from <infile>"<<endl;
        cout<<"       -o outfile   : to specify your own header file name"<<endl;
        cout<<"       -e extention : to specify your own header extension"<<endl;
        cout<<"       -l           : to print out just the list of charts in <infile>"<<endl;   
              
        return 0;
    }

    ifstream inFile(opt.in_filename.c_str());
    if ( !inFile.is_open() )
    {
        cout<<"Could not open file " << opt.in_filename << endl;
        return 0;
    }
    
    input_buffer inp;
    nout=0;
        
    // Read the whole file
    while(!inFile.eof())
    {
        getline(inFile, incoming);
        trim_trailing(incoming);
        if (opt.debug_on)
        {
            cout<< "Read line - " << incoming << endl;
        }
        
        if((!incoming.empty()) && (incoming.at(0)!=';'))
        {
            // Starting a new section when we see the []-line
            if (incoming.at(0) == '[' && incoming.at(incoming.length()-1) == ']')
            {
                // Already something picked up?
                if (!inp.empty())
                {
                    // Convert and save the current buffer before starting a new one
                    chart_name = inp.field("FN");
                    find_end = chart_name.find_last_of('.');
                    if (find_end != string::npos)
                    {
                        chart_name=chart_name.substr(0,find_end);
                    }                   
                    if (opt.list)
                    {
                        cout.width(15);
                        cout << left << chart_name;
                        cout << inp.field("NA") << endl;
                    }
                    else if ((opt.sw_single=="")||(chart_name==opt.sw_single))
                    {                
                        convert_section(inp, opt);
                        nout++;
                    }
                    inp.reset();
                }
            }
           
            if (incoming.at(0) == ' ')
            {
                if(!inp.append_line(trim(incoming))) ExitError("Bad Calibration File");
            }
            else
            {
               // Appending line to the buffer
               inp.add_line(trim(incoming));
            }
        }
    }

    inFile.close();
   
    // Convert the last section, if any
    if (!inp.empty())
    {
        // Convert and save the current buffer before starting a new one
        chart_name = inp.field("FN");
        find_end = chart_name.find_last_of('.');
        if (find_end != string::npos)
        {
            chart_name=chart_name.substr(0,find_end);
        }                   
        if (opt.list)
        {
            cout.width(15);
            cout << left << chart_name;
            cout << inp.field("NA") << endl;
        }
        else if ((opt.sw_single=="")||(chart_name==opt.sw_single))
        {                
            convert_section(inp, opt);
            nout++;
        }
        inp.reset();
    }
    if (nout>0) return 0;
    else        return 1;
}
