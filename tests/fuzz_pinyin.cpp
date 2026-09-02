#include "PinyinEngine.h"

#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <vector>

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
		if(!line.empty())
			known.insert(line);
	}
	return known;
}

static bool run_case(PinyinEngine &engine,const std::string &input,
		     const std::string &regression_path,std::set<std::string> &known,
		     int &new_failures)
{
	unsigned int commit_count = 0;
	unsigned int candidates = engine.search(input.c_str());
	std::string raw = engine.get_raw_pinyin();
	std::string display = engine.get_display_pinyin();
	bool pending = engine.has_pending_pinyin() || candidates == 0;

	bool ok_raw = (raw == input);
	bool ok_state = (candidates > 0 || pending);
	bool ok_commit = (commit_count == 0);

	if(ok_raw && ok_state && ok_commit)
		return true;

	std::cout << "FAIL input=" << input
		  << " display=" << display
		  << " raw=" << raw
		  << " candidates=" << candidates
		  << " pending=" << (pending ? "yes" : "no")
		  << " commit_count=" << commit_count
		  << " raw_ok=" << (ok_raw ? "yes" : "no")
		  << "\n";
	if(append_regression(regression_path,input,known))
		new_failures++;
	return false;
}

static bool expect_display(PinyinEngine &engine,const std::string &input,
			   const std::string &expected,
			   const std::string &regression_path,
			   std::set<std::string> &known,int &new_failures)
{
	unsigned int candidates = engine.search(input.c_str());
	std::string display = engine.get_display_pinyin();
	if(display == expected){
		std::cout << "FIXED input=" << input
			  << " display=" << display
			  << " candidates=" << candidates
			  << " pending=" << (engine.has_pending_pinyin() ? "yes" : "no")
			  << "\n";
		return true;
	}

	std::cout << "FAIL input=" << input
		  << " expected_display=" << expected
		  << " display=" << display
		  << " candidates=" << candidates
		  << " pending=" << (engine.has_pending_pinyin() ? "yes" : "no")
		  << "\n";
	if(append_regression(regression_path,input,known))
		new_failures++;
	return false;
}

int main(int argc,char **argv)
{
	const char *table = argc > 1 ? argv[1] : "scim/pinyin_table.txt";
	const char *phrase_index = argc > 2 ? argv[2] : "";
	const int rounds = argc > 3 ? atoi(argv[3]) : 20000;
	const std::string regression_path = argc > 4 ? argv[4] : "tests/fuzz_regressions.txt";

	srand(0x1f45);
	PinyinEngine engine(table,phrase_index);
	std::set<std::string> known = load_regressions(regression_path);

	std::vector<std::string> fixed;
	fixed.push_back("hef");
	fixed.push_back("nma");
	fixed.push_back("xian");
	fixed.push_back("beij");
	fixed.push_back("nihaoshijie");
	fixed.push_back("zhzh");
	fixed.push_back("ng");
	fixed.push_back("v");

	int total_failures = 0;
	int new_failures = 0;
	for(size_t i=0;i<fixed.size();i++){
		if(!run_case(engine,fixed[i],regression_path,known,new_failures))
			total_failures++;
	}

	if(!expect_display(engine,"hef","he f",regression_path,known,new_failures))
		total_failures++;
	if(!expect_display(engine,"nma","n ma",regression_path,known,new_failures))
		total_failures++;
	if(!expect_display(engine,"xian","xian",regression_path,known,new_failures))
		total_failures++;
	if(!expect_display(engine,"beij","bei j",regression_path,known,new_failures))
		total_failures++;
	if(!expect_display(engine,"nihaoshijie","ni hao shi jie",regression_path,known,new_failures))
		total_failures++;

	for(int i=0;i<rounds;i++){
		if(!run_case(engine,rand_input(),regression_path,known,new_failures))
			total_failures++;
	}

	std::cout << "cases=" << (rounds + (int)fixed.size())
		  << " failures=" << total_failures
		  << " new_failures=" << new_failures
		  << " regression_file=" << regression_path
		  << "\n";

	return new_failures == 0 ? 0 : 1;
}
