#include "persistence/project_serializer.hpp"
#include "timeline/timeline.hpp"
#include "core/log.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cctype>

namespace ve::persistence {

static std::string esc(const std::string& s){
    std::ostringstream o; for(char c: s){ if(c=='"'||c=='\\') o<<'\\'<<c; else if(c=='\n') o<<"\\n"; else o<<c;} return o.str(); }

SaveResult save_timeline_json(const timeline::Timeline& tl, const std::string& path) noexcept {
    std::ofstream ofs(path, std::ios::binary);
    if(!ofs) return {false, "Failed to open file for writing"};
    try {
        ofs << "{\n  \"version\":1,\n";
        ofs << "  \"name\": \""<< esc(tl.name()) <<"\",\n";
        ofs << "  \"frame_rate\": { \"num\": "<< tl.frame_rate().num <<", \"den\": "<< tl.frame_rate().den <<" },\n";
        ofs << "  \"tracks\": [\n";
        const auto& tracks = tl.tracks();
        for(size_t ti=0; ti<tracks.size(); ++ti){
            const auto& tr = tracks[ti];
            ofs << "    { \"id\": "<< tr->id() <<", \"type\": \""<<(tr->type()==timeline::Track::Video?"video":"audio")<<"\", \"name\": \""<<esc(tr->name())<<"\", \"segments\": [";
            const auto& segs = tr->segments();
            for(size_t si=0; si<segs.size(); ++si){
                const auto& s = segs[si];
                ofs << (si?",":"") << "{\"id\":"<< s.id <<",\"clip_id\":"<< s.clip_id <<",\"start_us\":"<< s.start_time.to_rational().num <<",\"dur_us\":"<< s.duration.to_rational().num <<"}";
            }
            ofs << "] }"<< (ti+1<tracks.size()?",":"") <<"\n";
        }
        ofs << "  ],\n  \"clips\": [";
        bool first=true; for(auto& [cid, clipPtr] : tl.clips()){
            if(!first) ofs << ","; first=false;
            auto inr = clipPtr->in_time.to_rational();
            auto outr = clipPtr->out_time.to_rational();
            ofs << "{\"id\":"<< cid <<",\"name\":\""<< esc(clipPtr->name) <<"\",\"in_us\":"<< inr.num <<",\"out_us\":"<< outr.num <<"}";
        }
        ofs << "]\n}";
        return {true,{}};
    } catch(const std::exception& e){ return {false, e.what()}; }
}

// Very small tokenizer for numbers, strings, punctuation; enough for our saved format
struct Tok { enum Type{Str, Num, LBrace, RBrace, LBrack, RBrack, Colon, Comma, End} type; std::string text; int64_t num=0; };
class Lexer { const std::string& s; size_t i=0; public: explicit Lexer(const std::string& s):s(s){} Tok next(){ while(i<s.size() && isspace((unsigned char)s[i])) ++i; if(i>=s.size()) return {Tok::End,{}}; char c=s[i++];
    switch(c){
        case '{': return {Tok::LBrace,"{"}; case '}': return {Tok::RBrace,"}"}; case '[': return {Tok::LBrack,"["}; case ']': return {Tok::RBrack,"]"}; case ':': return {Tok::Colon,":"}; case ',': return {Tok::Comma,","};
        case '"': { std::string out; while(i<s.size()){ char d=s[i++]; if(d=='\\' && i<s.size()){ out.push_back(s[i++]); continue; } if(d=='"') break; out.push_back(d);} return {Tok::Str,out}; }
        default: if(isdigit((unsigned char)c) || c=='-'){ bool neg = c=='-'; int64_t v=0; if(!neg) v=c-'0'; while(i<s.size() && isdigit((unsigned char)s[i])) { v = v*10 + (s[i]-'0'); ++i;} if(neg) v=-v; return {Tok::Num,{},v}; }
    }
    return next(); }
};

// Helper parsing routines -------------------------------------------------
static void parse_clips(Lexer& lex, timeline::Timeline& tl) {
    for(;;){ Tok t = lex.next(); if(t.type==Tok::RBrack || t.type==Tok::End) break; if(t.type!=Tok::LBrace) continue; // clip object
        int64_t cid=0,in_us=0,out_us=0; std::string cname; bool haveId=false; bool done=false;
        while(!done){ Tok k = lex.next(); if(k.type==Tok::Str){ std::string key=k.text; Tok c=lex.next(); if(c.type!=Tok::Colon) continue; Tok v=lex.next();
                if(key=="id" && v.type==Tok::Num){ cid=v.num; haveId=true; }
                else if(key=="name" && v.type==Tok::Str){ cname=v.text; }
                else if(key=="in_us" && v.type==Tok::Num){ in_us=v.num; }
                else if(key=="out_us" && v.type==Tok::Num){ out_us=v.num; }
            } else if(k.type==Tok::RBrace){ done=true; break; }
        }
        if(haveId){ auto source = std::make_shared<timeline::MediaSource>(); source->path=cname; source->duration = ve::TimeDuration{out_us,1}; tl.add_clip_with_id((timeline::ClipId)cid, source, cname, ve::TimePoint{in_us}, ve::TimePoint{out_us}); }
        Tok after = lex.next(); if(after.type==Tok::RBrack) break; }
}

static void parse_segments_array(Lexer& lex, timeline::Track* track){
    if(!track){ // consume till closing bracket
        int depth=1; while(depth>0){ Tok t=lex.next(); if(t.type==Tok::LBrack) ++depth; else if(t.type==Tok::RBrack) --depth; if(t.type==Tok::End) break; }
        return;
    }
    for(;;){ Tok t=lex.next(); if(t.type==Tok::RBrack || t.type==Tok::End) break; if(t.type!=Tok::LBrace) continue; timeline::Segment seg{}; bool done=false; while(!done){ Tok k=lex.next(); if(k.type==Tok::Str){ std::string key=k.text; Tok c=lex.next(); if(c.type!=Tok::Colon) continue; Tok v=lex.next(); if(key=="id" && v.type==Tok::Num) seg.id=(timeline::SegmentId)v.num; else if(key=="clip_id" && v.type==Tok::Num) seg.clip_id=(timeline::ClipId)v.num; else if(key=="start_us" && v.type==Tok::Num) seg.start_time=ve::TimePoint{v.num}; else if(key=="dur_us" && v.type==Tok::Num) seg.duration=ve::TimeDuration{v.num}; }
            else if(k.type==Tok::RBrace){ done=true; break; }
        }
    bool added = track->add_segment(seg); // best-effort add; overlaps logged internally
    if(!added){ ve::log::debug("Skipped adding overlapping or invalid segment id=" + std::to_string(seg.id)); }
        Tok after=lex.next(); if(after.type==Tok::RBrack) break; }
}

static void parse_tracks(Lexer& lex, timeline::Timeline& tl){
    for(;;){ Tok t=lex.next(); if(t.type==Tok::RBrack || t.type==Tok::End) break; if(t.type!=Tok::LBrace) continue; // track object
        timeline::Track::Type trackType = timeline::Track::Video; std::string tname; // removed unused haveName, segsPending & segStartPos
        // We'll collect segments after creating track when we meet segments key
        timeline::Track* trackPtr=nullptr; bool done=false;
        while(!done){ Tok k=lex.next(); if(k.type==Tok::Str){ std::string key=k.text; Tok c=lex.next(); if(c.type!=Tok::Colon) continue; Tok v=lex.next();
                if(key=="type" && v.type==Tok::Str){ trackType = (v.text=="audio"? timeline::Track::Audio: timeline::Track::Video); }
                else if(key=="name" && v.type==Tok::Str){ tname=v.text; haveName=true; }
                else if(key=="segments" && v.type==Tok::LBrack){ if(!trackPtr){ auto newId = tl.add_track(trackType, tname); trackPtr = tl.get_track(newId);} parse_segments_array(lex, trackPtr); }
                else { // skip primitive/object/array by naive consumption if needed
                    if(v.type==Tok::LBrace){ int depth=1; while(depth>0){ Tok tmp=lex.next(); if(tmp.type==Tok::LBrace) ++depth; else if(tmp.type==Tok::RBrace) --depth; if(tmp.type==Tok::End) break; } }
                    else if(v.type==Tok::LBrack){ int depth=1; while(depth>0){ Tok tmp=lex.next(); if(tmp.type==Tok::LBrack) ++depth; else if(tmp.type==Tok::RBrack) --depth; if(tmp.type==Tok::End) break; } }
                }
            } else if(k.type==Tok::RBrace){ done=true; break; }
        }
    if(!trackPtr){ (void)tl.add_track(trackType, tname); }
        Tok after=lex.next(); if(after.type==Tok::RBrack) break; }
}

LoadResult load_timeline_json(timeline::Timeline& tl, const std::string& path) noexcept {
    std::ifstream ifs(path, std::ios::binary); if(!ifs) return {false, "Failed to open file"};
    std::stringstream buf; buf << ifs.rdbuf(); std::string txt = buf.str();
    Lexer lex(txt); Tok t = lex.next(); if(t.type!=Tok::LBrace) return {false, "Expected root object"};
    int file_version = 0;
    while(true){ Tok k = lex.next(); if(k.type==Tok::End) break; if(k.type==Tok::RBrace) break; if(k.type!=Tok::Str){ continue; } std::string key=k.text; Tok colon=lex.next(); if(colon.type!=Tok::Colon) continue; Tok v=lex.next();
        if(key=="version" && v.type==Tok::Num){ file_version = (int)v.num; }
        else if(key=="name" && v.type==Tok::Str){ tl.set_name(v.text); }
        else if(key=="frame_rate" && v.type==Tok::LBrace){ int64_t num=0,den=0; bool haveN=false,haveD=false; while(true){ Tok nk=lex.next(); if(nk.type==Tok::Str){ std::string fk=nk.text; if(lex.next().type!=Tok::Colon) break; Tok nv=lex.next(); if(nv.type==Tok::Num){ if(fk=="num"){ num=nv.num; haveN=true;} else if(fk=="den"){ den=nv.num; haveD=true;} } } else if(nk.type==Tok::RBrace) break; }
            if(haveN && haveD && den!=0) tl.set_frame_rate({(int32_t)num,(int32_t)den}); }
        else if(key=="clips" && v.type==Tok::LBrack){ parse_clips(lex, tl); }
        else if(key=="tracks" && v.type==Tok::LBrack){ parse_tracks(lex, tl); }
        else { // skip value if it's complex structure not handled
            if(v.type==Tok::LBrace){ int depth=1; while(depth>0){ Tok tmp=lex.next(); if(tmp.type==Tok::LBrace) ++depth; else if(tmp.type==Tok::RBrace) --depth; if(tmp.type==Tok::End) break; } }
            else if(v.type==Tok::LBrack){ int depth=1; while(depth>0){ Tok tmp=lex.next(); if(tmp.type==Tok::LBrack) ++depth; else if(tmp.type==Tok::RBrack) --depth; if(tmp.type==Tok::End) break; } }
        }
        Tok sep=lex.next(); if(sep.type==Tok::RBrace) break; if(sep.type==Tok::End) break; }
    if(file_version>1){ return {false, "Unsupported project version"}; }
    return {true,{}};
}

} // namespace ve::persistence
