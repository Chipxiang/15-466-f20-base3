#include "Mode.hpp"

#include "Scene.hpp"
#include "Sound.hpp"

#include <glm/glm.hpp>
#include <math.h>

#include <vector>
#include <deque>
#include <string>

struct PlayMode : Mode {
	PlayMode();
	virtual ~PlayMode();

	//functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;
	virtual void check_player_pos(float x, float y);
	virtual void reset_game(bool ending);

	//----- game state -----
	static const int GRID_SIZE = 16;
	static const int UNIT_SIZE = 2;
	bool grid[GRID_SIZE][GRID_SIZE] = {false};
	int start = 0;
	int end = 0;
	//input tracking:
	struct Button {
		uint8_t downs = 0;
		uint8_t pressed = 0;
	} left, right, down, up;

	//local copy of the game scene (so code can change it during gameplay):
	Scene scene;

	//hexapod leg to wobble:
	Scene::Transform *hip = nullptr;
	Scene::Transform *upper_leg = nullptr;
	Scene::Transform *lower_leg = nullptr;
	glm::quat hip_base_rotation;
	glm::quat upper_leg_base_rotation;
	glm::quat lower_leg_base_rotation;
	float wobble = 0.0f;
	
	Scene::Transform *player = nullptr;
	Scene::Transform *target = nullptr;
	std::vector<Scene::Transform*> cube_vec;
	Scene::Transform *wrong_cube = nullptr;

	int player_pos = 0;
	glm::ivec2 player_pos_2d = glm::ivec2(0,0);
	bool stop_move = false;
	float wrong_timer = 0.0f;
	float grace_time = 2.0f;
	int score = 0;
	float ending_timer = 0.0f;

	glm::ivec2 target_position;
	glm::vec3 get_leg_tip_position();
	void randomize_grid();
	std::deque<glm::ivec2> path;
	//sound
	std::shared_ptr< Sound::PlayingSample > target_loop;
	void play_death_effect();
	void play_victory_effect();
	//camera:
	Scene::Camera *camera = nullptr;
	glm::vec3 camera_origin_pos = glm::vec3(0.0f,0.0f,0.0f);

};
