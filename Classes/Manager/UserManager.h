#ifndef USER_MANAGER_H
#define USER_MANAGER_H

#include "cocos2d.h"
#include<vector>
#include<map>
USING_NS_CC;

class UserManager
{
public:
	UserManager();

	static UserManager* getInstance();

	static UserManager* _instance;

	std::string SkinRandom();

	std::string FaceRandom();

	std::string HatRandom();

	std::vector<std::string> UserName;

	std::map<std::string, int> User_marks;

	std::map<std::string, std::vector<double>>Standing;

	std::map<int, std::string> player_name;

	std::map<int, bool> player_type;

	std::map<int, std::string> player_skin;

	std::map<int, std::string> player_face;

	std::map<int, std::string> player_hat;

	int Live;
};

#endif
