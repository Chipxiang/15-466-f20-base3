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

	//----- game state -----
	static const int GRID_SIZE = 16;
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
	std::vector<Scene::Transform*> cube_vec;

	int player_pos = 0;

	glm::vec3 get_leg_tip_position();
	void randomize_grid();
	//music coming from the target:
	std::shared_ptr< Sound::PlayingSample > target_loop;
	
	//camera:
	Scene::Camera *camera = nullptr;

};
