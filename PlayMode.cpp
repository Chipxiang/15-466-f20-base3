#include "PlayMode.hpp"

#include "LitColorTextureProgram.hpp"

#include "DrawLines.hpp"
#include "Mesh.hpp"
#include "Load.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <random>

GLuint game_scene_meshes_for_lit_color_texture_program = 0;
Load< MeshBuffer > game_scene_meshes(LoadTagDefault, []() -> MeshBuffer const * {
	MeshBuffer const *ret = new MeshBuffer(data_path("game_map.pnct"));
	game_scene_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
});

Load< Scene > game_scene(LoadTagDefault, []() -> Scene const * {
	return new Scene(data_path("game_map.scene"), [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
		Mesh const &mesh = game_scene_meshes->lookup(mesh_name);

		scene.drawables.emplace_back(transform);
		Scene::Drawable &drawable = scene.drawables.back();

		drawable.pipeline = lit_color_texture_program_pipeline;

		drawable.pipeline.vao = game_scene_meshes_for_lit_color_texture_program;
		drawable.pipeline.type = mesh.type;
		drawable.pipeline.start = mesh.start;
		drawable.pipeline.count = mesh.count;

	});
});

Load< Sound::Sample > organ_filler_sample(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("Organ-Filler.opus"));
});

PlayMode::PlayMode() : scene(*game_scene) {
	//get pointers to player for convenience:
	std::string cube_prefix = "Cube";
	for (auto &transform : scene.transforms) {
		if (transform.name == "player") player = &transform;
		else if (transform.name.find(cube_prefix) == 0){
			std::cout<<"find cube "<<transform.name<<std::endl;
			cube_vec.push_back(&transform);
		}
		else if (transform.name == "target"){
			target = &transform;
		}
	}
	if (player == nullptr) throw std::runtime_error("player not found.");
	if (target == nullptr) throw std::runtime_error("target not found.");
	randomize_grid();
	check_player_pos(player->position.x, player->position.y);
	std::cout << player->position.x << "," << player->position.y << "," << player->position.z << std::endl;
	std::cout << player->rotation.x << "," << player->rotation.y << "," << player->rotation.z << std::endl;

	player->position.x += path[0].x * UNIT_SIZE;
	player->position.y += path[0].y * UNIT_SIZE;

	//get pointer to camera for convenience:
	if (scene.cameras.size() != 1) throw std::runtime_error("Expecting scene to have exactly one camera, but it has " + std::to_string(scene.cameras.size()));
	camera = &scene.cameras.front();
	//camera->transform->position = glm::vec3(0, -7, 5);
	//camera->transform-> parent = player;
	std::cout << camera->transform->position.x << "," << camera->transform->position.y << "," << camera->transform->position.z << std::endl;
	std::cout << camera->transform->rotation.x << "," << camera->transform->rotation.y << "," << camera->transform->rotation.z << std::endl;

	camera->transform->position.x += path[0].x * UNIT_SIZE;
	camera->transform->position.y += path[0].y * UNIT_SIZE;
	//start music loop playing:
	// (note: position will be over-ridden in update())
	target_position = path[1];
	target_loop = Sound::loop_3D(*organ_filler_sample, 1.0f, glm::vec3(target_position.x * UNIT_SIZE, target_position.y * UNIT_SIZE,0), 1.0f);

	for (int i = GRID_SIZE - 1; i > 0; i--) {
		for (int j = 0; j < GRID_SIZE; j++) {
			std::cout << grid[j][i] << " ";
		}
		std::cout << std::endl;
	}
}
PlayMode::~PlayMode() {
	
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {

	if (evt.type == SDL_KEYDOWN) {
		if (evt.key.keysym.sym == SDLK_ESCAPE) {
			SDL_SetRelativeMouseMode(SDL_FALSE);
			return true;
		} else if (evt.key.keysym.sym == SDLK_a) {
			left.downs += 1;
			left.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_d) {
			right.downs += 1;
			right.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_w) {
			up.downs += 1;
			up.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_s) {
			down.downs += 1;
			down.pressed = true;
			return true;
		}
	} else if (evt.type == SDL_KEYUP) {
		if (evt.key.keysym.sym == SDLK_a) {
			left.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_d) {
			right.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_w) {
			up.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_s) {
			down.pressed = false;
			return true;
		}
	} else if (evt.type == SDL_MOUSEBUTTONDOWN) {
		if (SDL_GetRelativeMouseMode() == SDL_FALSE) {
			SDL_SetRelativeMouseMode(SDL_TRUE);
			return true;
		}
	} else if (evt.type == SDL_MOUSEMOTION) {
		if (SDL_GetRelativeMouseMode() == SDL_TRUE) {
			glm::vec2 motion = glm::vec2(
				evt.motion.xrel / float(window_size.y),
				-evt.motion.yrel / float(window_size.y)
			);
			camera->transform->rotation = glm::normalize(
				camera->transform->rotation
				* glm::angleAxis(-motion.x * camera->fovy, glm::vec3(0.0f, 1.0f, 0.0f))
				* glm::angleAxis(motion.y * camera->fovy, glm::vec3(1.0f, 0.0f, 0.0f))
			);
			return true;
		}
	}

	return false;
}

void PlayMode::check_player_pos(float x, float y){
	int cord_x = int(floor((x+1)/2));
	int cord_y = int(floor((y+1)/2)); 
	player_pos = cord_x+cord_y*16;
	player_pos_2d = glm::ivec2(cord_x,cord_y);
}

void PlayMode::update(float elapsed) {

	// //move sound to follow leg tip position:
	// leg_tip_loop->set_position(get_leg_tip_position(), 1.0f / 60.0f);

	//move camera:
	{

		//combine inputs into a move:
		constexpr float PlayerSpeed = 3.0f;
		glm::vec3 move = glm::vec3(0.0f);
		if (left.pressed && !right.pressed) move.x =-1.0f;
		if (!left.pressed && right.pressed) move.x = 1.0f;
		if (down.pressed && !up.pressed) move.y =-1.0f;
		if (!down.pressed && up.pressed) move.y = 1.0f;

		if (move != glm::vec3(0.0f)) {
			player->rotation = glm::quatLookAt(-glm::normalize(move), glm::vec3(0, 0, 1));
			move = glm::normalize(move) * PlayerSpeed * elapsed;
			player->position += move;
			camera->transform->position += move;
		}

		check_player_pos(player->position.x, player->position.y);
		//make it so that moving diagonally doesn't go faster:

		/*
		glm::mat4x3 frame = camera->transform->make_local_to_parent();
		glm::vec3 right = frame[0];
		//glm::vec3 up = frame[1];
		glm::vec3 forward = -frame[2];

		camera->transform->position += move.x * right + move.y * forward;*/
	}

	/*{ //update listener to camera position:
		glm::mat4x3 frame = camera->transform->make_local_to_parent();
		glm::vec3 right = frame[0];
		glm::vec3 at = frame[3];
		Sound::listener.set_position_right(at, right, 1.0f / 60.0f);
	}*/
	{
		glm::mat4x3 frame = camera->transform->make_local_to_parent();
		glm::vec3 right = frame[0];
		Sound::listener.set_position_right(player->position, right, 1.0f / 60.0f );

	}

	//reset button press counters:
	left.downs = 0;
	right.downs = 0;
	up.downs = 0;
	down.downs = 0;
}

void PlayMode::draw(glm::uvec2 const &drawable_size) {
	//update camera aspect ratio for drawable:
	camera->aspect = float(drawable_size.x) / float(drawable_size.y);

	//set up light type and position for lit_color_texture_program:
	// TODO: consider using the Light(s) in the scene to do this

	glUseProgram(lit_color_texture_program->program);
	glUniform1i(lit_color_texture_program->LIGHT_TYPE_int, 2);
	glUniform3fv(lit_color_texture_program->LIGHT_LOCATION_vec3, 1, glm::value_ptr(player->position + (glm::vec3(0.0f,0.0f, 10.f))));
	glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f, -1.0f)));
	glUniform1f(lit_color_texture_program->LIGHT_CUTOFF_float, std::cos(3.1415926f * 0.025f));
	glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(50.0f * glm::vec3(1.0f, 1.0f, 0.95f)));
	glUseProgram(0);
	
	/*glUseProgram(lit_color_texture_program->program);
	glUniform1i(lit_color_texture_program->LIGHT_TYPE_int, 1);
	glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f,-1.0f)));
	glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 0.95f)));
	glUseProgram(0);*/

	glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	glClearDepth(1.0f); //1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS); //this is the default depth comparison function, but FYI you can change it.

	scene.draw(*camera);

	{ //use DrawLines to overlay some text:
		glDisable(GL_DEPTH_TEST);
		float aspect = float(drawable_size.x) / float(drawable_size.y);
		DrawLines lines(glm::mat4(
			1.0f / aspect, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		));

		constexpr float H = 0.09f;
		lines.draw_text("Mouse motion rotates camera; WASD moves; escape ungrabs mouse",
			glm::vec3(-aspect + 0.1f * H, -1.0 + 0.1f * H, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0x00, 0x00, 0x00, 0x00));
		float ofs = 2.0f / drawable_size.y;
		lines.draw_text("Mouse motion rotates camera; WASD moves; escape ungrabs mouse",
			glm::vec3(-aspect + 0.1f * H + ofs, -1.0 + + 0.1f * H + ofs, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0x00));
	}
	GL_ERRORS();
}

glm::vec3 PlayMode::get_leg_tip_position() {
	//the vertex position here was read from the model in blender:
	return lower_leg->make_local_to_world() * glm::vec4(-1.26137f, -11.861f, 0.0f, 1.0f);
}

void PlayMode::randomize_grid()
{
	for (int i = 0; i < GRID_SIZE; i++) {
		for (int j = 0; j < GRID_SIZE; j++) {
			grid[i][j] = false;
		}
	}
	std::random_device rd;
	std::mt19937 mt(rd());
	std::uniform_int_distribution<int> start_rand(0, GRID_SIZE - 1);
	std::uniform_int_distribution<int> direction_rand(0, 2);
	start = start_rand(mt);
	grid[0][start] = true;
	int curr_x = 0;
	int curr_y = start;
	path.clear();
	path.push_back(glm::vec2(curr_x, curr_y));
	while (curr_x != GRID_SIZE - 1) {
		int direction = direction_rand(mt);
		switch (direction) {
		case 0:
			curr_x += 1;
			break;
		case 1:
			if (curr_y - 1 < 0) {
				continue;
			}
			if (!grid[curr_x][curr_y - 1]) {
				curr_y--;
				break;
			}
		case 2:
			if (curr_y + 1 >= GRID_SIZE) {
				continue;
			}
			if (!grid[curr_x][curr_y + 1]) {
				curr_y++;
				break;
			}
		default:
			curr_x++;
			break;
		}
		grid[curr_x][curr_y] = true;
		path.push_back(glm::vec2(curr_x, curr_y));
	}
	grid[curr_x][curr_y] = true;
	end = curr_y;


}
