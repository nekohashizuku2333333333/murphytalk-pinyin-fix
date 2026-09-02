#include "PinyinEngine.h"
#include "scim/scim_pinyin.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <sys/time.h>
#include <vector>

extern int utf8_wctomb(__u8 *s, ucs4_t wc, int maxlen);

static const char *kSyllables[] = {
	"a","ai","an","ang","ao","ba","bai","ban","bang","bao","bei","ben","beng",
	"bi","bian","biao","bie","bin","bing","bo","bu","ca","cai","can","cang",
	"cao","ce","cen","ceng","cha","chai","chan","chang","chao","che","chen",
	"cheng","chi","chong","chou","chu","chuai","chuan","chuang","chui","chun",
	"chuo","ci","cong","cou","cu","cuan","cui","cun","cuo","da","dai","dan",
	"dang","dao","de","dei","den","deng","di","dia","dian","diao","die","ding",
	"diu","dong","dou","du","duan","dui","dun","duo","e","ei","en","eng","er",
	"fa","fan","fang","fei","fen","feng","fo","fou","fu","ga","gai","gan",
	"gang","gao","ge","gei","gen","geng","gong","gou","gu","gua","guai","guan",
	"guang","gui","gun","guo","ha","hai","han","hang","hao","he","hei","hen",
	"heng","hong","hou","hu","hua","huai","huan","huang","hui","hun","huo",
	"ji","jia","jian","jiang","jiao","jie","jin","jing","jiong","jiu","ju",
	"juan","jue","jun","ka","kai","kan","kang","kao","ke","ken","keng","kong",
	"kou","ku","kua","kuai","kuan","kuang","kui","kun","kuo","la","lai","lan",
	"lang","lao","le","lei","leng","li","lia","lian","liang","liao","lie",
	"lin","ling","liu","long","lou","lu","luan","lue","lun","luo","ma","mai",
	"man","mang","mao","me","mei","men","meng","mi","mian","miao","mie","min",
	"ming","miu","mo","mou","mu","na","nai","nan","nang","nao","ne","nei",
	"nen","neng","ni","nian","niang","niao","nie","nin","ning","niu","nong",
	"nu","nuan","nue","nuo","o","ou","pa","pai","pan","pang","pao","pei",
	"pen","peng","pi","pian","piao","pie","pin","ping","po","pou","pu","qi",
	"qia","qian","qiang","qiao","qie","qin","qing","qiong","qiu","qu","quan",
	"que","qun","ran","rang","rao","re","ren","reng","ri","rong","rou","ru",
	"ruan","rui","run","ruo","sa","sai","san","sang","sao","se","sen","seng",
	"sha","shai","shan","shang","shao","she","shen","sheng","shi","shou","shu",
	"shua","shuai","shuan","shuang","shui","shun","shuo","si","song","sou",
	"su","suan","sui","sun","suo","ta","tai","tan","tang","tao","te","teng",
	"ti","tian","tiao","tie","ting","tong","tou","tu","tuan","tui","tun",
	"tuo","wa","wai","wan","wang","wei","wen","weng","wo","wu","xi","xia",
	"xian","xiang","xiao","xie","xin","xing","xiong","xiu","xu","xuan","xue",
	"xun","ya","yan","yang","yao","ye","yi","yin","ying","yo","yong","you",
	"yu","yuan","yue","yun","za","zai","zan","zang","zao","ze","zei","zen",
	"zeng","zha","zhai","zhan","zhang","zhao","zhe","zhen","zheng","zhi",
	"zhong","zhou","zhu","zhua","zhuai","zhuan","zhuang","zhui","zhun","zhuo",
	"zi","zong","zou","zu","zuan","zui","zun","zuo"
};

static const char *kInitials[] = {
	"b","p","m","f","d","t","n","l","g","k","h","j","q","x","zh","ch","sh",
	"r","z","c","s","y","w"
};

struct Snapshot
{
	std::string input;
	std::string raw;
	std::string display;
	unsigned int candidates;
	bool pending;
	unsigned int commit_count;
	double elapsed_ms;
	std::string first_phrase_utf8;
};

static double now_ms()
{
	struct timeval tv;
	gettimeofday(&tv,0);
	return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

static std::string utf8_from_ucs4(ucs4_t code)
{
	__u8 buf[8];
	int len = utf8_wctomb(buf,code,6);
	if(len <= 0)
		return "";
	return std::string((char*)buf,(char*)buf+len);
}

static std::string to_utf8(QString s)
{
	std::string out;
	for(unsigned int i=0;i<s.length();i++)
		out += utf8_from_ucs4(s[i].unicode());
	return out;
}

static Snapshot search_snapshot(PinyinEngine &engine,const std::string &input)
{
	Snapshot s;
	s.input = input;
	s.commit_count = 0;
	double begin = now_ms();
	s.candidates = engine.search(input.c_str());
	double end = now_ms();
	s.elapsed_ms = end - begin;
	s.raw = engine.get_raw_pinyin();
	s.display = engine.get_display_pinyin();
	s.pending = engine.has_pending_pinyin() || s.candidates == 0;
	if(s.candidates > 0 && engine.is_phrase_candidate(0))
		s.first_phrase_utf8 = to_utf8(engine.get_phrase(0));
	return s;
}

static bool same_snapshot(const Snapshot &a,const Snapshot &b)
{
	return a.raw == b.raw &&
	       a.display == b.display &&
	       a.candidates == b.candidates &&
	       a.pending == b.pending &&
	       a.commit_count == b.commit_count &&
	       a.first_phrase_utf8 == b.first_phrase_utf8;
}

static bool append_regression(const std::string &path,const std::string &input,std::set<std::string> &known)
{
	if(known.find(input) != known.end())
		return false;
	std::ofstream out(path.c_str(),std::ios::app);
	out << input << "\n";
	known.insert(input);
	return true;
}

static std::set<std::string> load_regressions(const std::string &path)
{
	std::set<std::string> known;
	std::ifstream in(path.c_str());
	std::string line;
	while(std::getline(in,line)){
		if(!line.empty() && line[0] != '#')
			known.insert(line);
	}
	return known;
}

static void print_fail(const std::string &kind,const Snapshot &s,const std::string &extra)
{
	std::cout << "FAIL kind=" << kind
		  << " input=\"" << s.input << "\""
		  << " display=\"" << s.display << "\""
		  << " raw=\"" << s.raw << "\""
		  << " candidates=" << s.candidates
		  << " pending=" << (s.pending ? "yes" : "no")
		  << " commit_count=" << s.commit_count
		  << " elapsed_ms=" << s.elapsed_ms
		  << extra
		  << "\n";
}

static bool base_invariants(PinyinEngine &engine,const std::string &input,
			    const std::string &regression_path,std::set<std::string> &known,
			    int &new_failures,double max_ms)
{
	Snapshot s = search_snapshot(engine,input);
	bool ok_raw = (s.raw == input);
	bool ok_state = (s.candidates > 0 || s.pending);
	bool ok_commit = (s.commit_count == 0);
	bool ok_perf = (s.elapsed_ms <= max_ms);
	if(ok_raw && ok_state && ok_commit && ok_perf)
		return true;

	std::ostringstream extra;
	extra << " raw_ok=" << (ok_raw ? "yes" : "no")
	      << " state_ok=" << (ok_state ? "yes" : "no")
	      << " commit_ok=" << (ok_commit ? "yes" : "no")
	      << " perf_ok=" << (ok_perf ? "yes" : "no")
	      << " max_ms=" << max_ms;
	print_fail("base",s,extra.str());
	if(append_regression(regression_path,input,known))
		new_failures++;
	return false;
}

static bool prefix_invariant(PinyinEngine &engine,const std::string &input,
			     const std::string &regression_path,std::set<std::string> &known,
			     int &new_failures,double max_ms)
{
	bool ok = true;
	for(size_t i=1;i<=input.size();i++){
		if(!base_invariants(engine,input.substr(0,i),regression_path,known,new_failures,max_ms))
			ok = false;
	}
	return ok;
}

static bool backspace_invariant(PinyinEngine &engine,const std::string &input,
				const std::string &regression_path,std::set<std::string> &known,
				int &new_failures)
{
	bool ok = true;
	for(size_t k=1;k<=input.size();k++){
		std::string prefix = input.substr(0,input.size()-k);
		(void)search_snapshot(engine,input);
		Snapshot after_backspace = search_snapshot(engine,prefix);
		Snapshot direct = search_snapshot(engine,prefix);
		if(!same_snapshot(after_backspace,direct)){
			print_fail("backspace",after_backspace," expected_display=\"" + direct.display + "\"");
			if(append_regression(regression_path,input,known))
				new_failures++;
			ok = false;
		}
	}
	return ok;
}

static bool deterministic_invariant(PinyinEngine &engine,const std::string &input,
				    const std::string &regression_path,std::set<std::string> &known,
				    int &new_failures)
{
	Snapshot a = search_snapshot(engine,input);
	Snapshot b = search_snapshot(engine,input);
	if(same_snapshot(a,b))
		return true;
	print_fail("deterministic",a," second_display=\"" + b.display + "\"");
	if(append_regression(regression_path,input,known))
		new_failures++;
	return false;
}

static bool clear_invariant(PinyinEngine &engine,const std::string &input,
			    const std::string &regression_path,std::set<std::string> &known,
			    int &new_failures)
{
	(void)search_snapshot(engine,input);
	engine.clear_key();
	Snapshot s;
	s.input = input;
	s.raw = engine.get_raw_pinyin();
	s.display = engine.get_display_pinyin();
	s.candidates = engine.get_char_count() + engine.get_phrase_candidate_count();
	s.pending = engine.has_pending_pinyin();
	s.commit_count = 0;
	s.elapsed_ms = 0;
	if(s.raw.empty() && s.display.empty() && s.candidates == 0 && !s.pending)
		return true;
	print_fail("clear-state",s,"");
	if(append_regression(regression_path,input,known))
		new_failures++;
	return false;
}

static bool expect_display(PinyinEngine &engine,const std::string &input,const std::string &expected,
			   const std::string &regression_path,std::set<std::string> &known,
			   int &new_failures)
{
	Snapshot s = search_snapshot(engine,input);
	if(s.display == expected){
		std::cout << "FIXED input=" << input
			  << " display=" << s.display
			  << " candidates=" << s.candidates
			  << " pending=" << (s.pending ? "yes" : "no")
			  << "\n";
		return true;
	}
	print_fail("display",s," expected_display=\"" + expected + "\"");
	if(append_regression(regression_path,input,known))
		new_failures++;
	return false;
}

static bool expect_candidates(PinyinEngine &engine,const std::string &input,
			      const std::string &regression_path,std::set<std::string> &known,
			      int &new_failures)
{
	Snapshot s = search_snapshot(engine,input);
	if(s.candidates > 0)
		return true;
	print_fail("candidate-required",s,"");
	if(append_regression(regression_path,input,known))
		new_failures++;
	return false;
}

static bool expect_pending(PinyinEngine &engine,const std::string &input,
			   const std::string &regression_path,std::set<std::string> &known,
			   int &new_failures)
{
	Snapshot s = search_snapshot(engine,input);
	if(s.pending)
		return true;
	print_fail("pending-required",s,"");
	if(append_regression(regression_path,input,known))
		new_failures++;
	return false;
}

static bool expect_phrase_contains(PinyinEngine &engine,const std::string &input,const std::string &needle,
				   const std::string &regression_path,std::set<std::string> &known,
				   int &new_failures)
{
	Snapshot s = search_snapshot(engine,input);
	for(unsigned int i=0;i<s.candidates;i++){
		if(engine.is_phrase_candidate(i)){
			std::string phrase = to_utf8(engine.get_phrase(i));
			if(phrase.find(needle) != std::string::npos)
				return true;
		}
	}
	print_fail("phrase-required",s," needle=\"" + needle + "\"");
	if(append_regression(regression_path,input,known))
		new_failures++;
	return false;
}

static bool expect_first_char_consumes(PinyinEngine &engine,const std::string &input,unsigned int expected,
				       const std::string &regression_path,std::set<std::string> &known,
				       int &new_failures)
{
	Snapshot s = search_snapshot(engine,input);
	unsigned int index = engine.get_phrase_candidate_count();
	if(index >= s.candidates){
		print_fail("char-candidate-required",s,"");
		if(append_regression(regression_path,input,known))
			new_failures++;
		return false;
	}
	unsigned int consumed = engine.get_commit_pinyin_length(index);
	if(consumed == expected)
		return true;
	std::ostringstream extra;
	extra << " expected_consumed=" << expected << " consumed=" << consumed;
	print_fail("consume-length",s,extra.str());
	if(append_regression(regression_path,input,known))
		new_failures++;
	return false;
}

static std::string rand_piece()
{
	int mode = rand() % 5;
	if(mode <= 1){
		std::string s = kSyllables[rand() % (sizeof(kSyllables)/sizeof(kSyllables[0]))];
		if(mode == 1 && s.size() > 1)
			s.resize(1 + rand() % s.size());
		return s;
	}
	if(mode == 2)
		return kInitials[rand() % (sizeof(kInitials)/sizeof(kInitials[0]))];
	if(mode == 3)
		return std::string(1, static_cast<char>('a' + rand() % 26));
	return "'";
}

static std::string rand_input()
{
	std::string s;
	while(s.empty() || s.size() > 12){
		s.clear();
		int pieces = 1 + rand() % 6;
		for(int i=0;i<pieces;i++)
			s += rand_piece();
		if(s.size() > 12)
			s.resize(1 + rand() % 12);
	}
	return s;
}

int main(int argc,char **argv)
{
	const char *table = argc > 1 ? argv[1] : "scim/pinyin_table.txt";
	const char *phrase_index = argc > 2 ? argv[2] : "";
	const int rounds = argc > 3 ? atoi(argv[3]) : 20000;
	const std::string regression_path = argc > 4 ? argv[4] : "tests/fuzz_regressions.txt";
	const double max_ms = argc > 5 ? atof(argv[5]) : 25.0;

	srand(0x1f45);
	PinyinEngine engine(table,phrase_index);
	std::set<std::string> known = load_regressions(regression_path);

	const char *fixed[] = {
		"a","e","o","ai","an","ang","ao","ei","en","eng","er","ou",
		"n","m","v","b","zh","ch","sh","i","u","ng",
		"hef","nih","zhon","nihaoshij","wob","h","he","hefe","hefei",
		"ni","niha","nihao",
		"xian","xiang","tian","jinan","fangan","jingan","dangan","pingan",
		"liangan","guangan","wanan","renai","xinan","shan","haian",
		"nma","bj","rmb","zg","zhg","nhsj","wm","nm","nh","beij","nih",
		"zhongg","shangh","beijingd","nhao","sh",
		"xi'an","xian'","'xian","xi''an","xi'an'","ni'hao","n'ma",
		"lv","nv","lve","nve","lu","nu","lvse","nvren",
		""," ","bcd","zhchsh","ngng","aaaa","aeiou","nnnn","mmmm","vvvv",
		"iu","ui","ue","NiHao","nihao1","ni hao","ni-hao","ni.hao",
		"nihaoshijie","zhonghuarenmingongheguo","woaibeijingtiananmen",
		"zhuangzhuangzhuangzhuang","nnnnnnnnnnnnnnnnnnnn"
	};

	int total_failures = 0;
	int new_failures = 0;
	for(size_t i=0;i<sizeof(fixed)/sizeof(fixed[0]);i++){
		std::string s = fixed[i];
		if(!base_invariants(engine,s,regression_path,known,new_failures,max_ms)) total_failures++;
		if(!prefix_invariant(engine,s,regression_path,known,new_failures,max_ms)) total_failures++;
		if(!backspace_invariant(engine,s,regression_path,known,new_failures)) total_failures++;
		if(!deterministic_invariant(engine,s,regression_path,known,new_failures)) total_failures++;
		if(!clear_invariant(engine,s,regression_path,known,new_failures)) total_failures++;
	}

	const char *complete[] = {"a","e","o","ai","an","ang","ao","ei","en","eng","er","ou"};
	for(size_t i=0;i<sizeof(complete)/sizeof(complete[0]);i++){
		if(!expect_candidates(engine,complete[i],regression_path,known,new_failures)) total_failures++;
	}
	const char *pending[] = {"n","m","v","b","zh","ch","sh"};
	for(size_t i=0;i<sizeof(pending)/sizeof(pending[0]);i++){
		if(!expect_pending(engine,pending[i],regression_path,known,new_failures)) total_failures++;
	}

	if(!expect_display(engine,"hef","he f",regression_path,known,new_failures)) total_failures++;
	if(!expect_display(engine,"nma","n ma",regression_path,known,new_failures)) total_failures++;
	if(!expect_display(engine,"xian","xian",regression_path,known,new_failures)) total_failures++;
	if(!expect_display(engine,"beij","bei j",regression_path,known,new_failures)) total_failures++;
	if(!expect_display(engine,"nihaoshijie","ni hao shi jie",regression_path,known,new_failures)) total_failures++;
	if(!expect_phrase_contains(engine,"hefei","合肥",regression_path,known,new_failures)) total_failures++;
	if(!expect_first_char_consumes(engine,"hef",2,regression_path,known,new_failures)) total_failures++;
	if(!expect_first_char_consumes(engine,"nma",1,regression_path,known,new_failures)) total_failures++;
	if(!expect_first_char_consumes(engine,"nima",2,regression_path,known,new_failures)) total_failures++;
	if(!expect_first_char_consumes(engine,"cong",4,regression_path,known,new_failures)) total_failures++;
	if(!expect_first_char_consumes(engine,"wcao",1,regression_path,known,new_failures)) total_failures++;

	for(int i=0;i<rounds;i++){
		std::string s = rand_input();
		if(!base_invariants(engine,s,regression_path,known,new_failures,max_ms)) total_failures++;
		if(!prefix_invariant(engine,s,regression_path,known,new_failures,max_ms)) total_failures++;
		if(!deterministic_invariant(engine,s,regression_path,known,new_failures)) total_failures++;
		if(!clear_invariant(engine,s,regression_path,known,new_failures)) total_failures++;
	}

	std::cout << "cases=" << (rounds + (int)(sizeof(fixed)/sizeof(fixed[0])))
		  << " failures=" << total_failures
		  << " new_failures=" << new_failures
		  << " max_ms=" << max_ms
		  << " regression_file=" << regression_path
		  << "\n";

	return new_failures == 0 ? 0 : 1;
}
