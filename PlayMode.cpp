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
Load< Sound::Sample > maybe_next_time_sample(LoadTagDefault, []() -> Sound::Sample const* {
	return new Sound::Sample(data_path("maybe_next_time.opus"));
	});
Load< Sound::Sample > nice_one_sample(LoadTagDefault, []() -> Sound::Sample const* {
	return new Sound::Sample(data_path("nice_one.opus"));
	});
PlayMode::PlayMode() : scene(*game_scene) {
	//get pointers to player for convenience:
	std::string cube_prefix = "Cube";
	for (auto &transform : scene.transforms) {
		if (transform.name == "player") player = &transform;
		else if (transform.name.find(cube_prefix) == 0){
			cube_vec.push_back(&transform);
		}
		else if (transform.name == "target"){
			target = &transform;
		}
	}
	if (player == nullptr) throw std::runtime_error("player not found.");
	if (target == nullptr) throw std::runtime_error("target not found.");
	randomize_grid();
	std::cout << player->position.x << "," << player->position.y << "," << player->position.z << std::endl;
	std::cout << player->rotation.x << "," << player->rotation.y << "," << player->rotation.z << std::endl;
	glm::ivec2 initial_player_pos = path.front();
	path.pop_front();
	player->position.x += initial_player_pos.x * UNIT_SIZE;
	player->position.y += initial_player_pos.y * UNIT_SIZE;
	check_player_pos(player->position.x, player->position.y);

	//get pointer to camera for convenience:
	if (scene.cameras.size() != 1) throw std::runtime_error("Expecting scene to have exactly one camera, but it has " + std::to_string(scene.cameras.size()));
	camera = &scene.cameras.front();
	camera_origin_pos = camera->transform->position;
	//camera->transform->position = glm::vec3(0, -7, 5);
	//camera->transform-> parent = player;
	std::cout << camera->transform->position.x << "," << camera->transform->position.y << "," << camera->transform->position.z << std::endl;
	std::cout << camera->transform->rotation.x << "," << camera->transform->rotation.y << "," << camera->transform->rotation.z << std::endl;

	camera->transform->position.x += initial_player_pos.x * UNIT_SIZE;
	camera->transform->position.y += initial_player_pos.y * UNIT_SIZE;
	//start music loop playing:
	// (note: position will be over-ridden in update())
	target_position = path.front();
	target->rotation = glm::quatLookAt(-glm::normalize(glm::vec3(-1,-1,0)), glm::vec3(0, 0, 1));

	path.pop_front();
	target->position = glm::vec3(target_position.x * UNIT_SIZE, target_position.y * UNIT_SIZE, 0);
	target_loop = Sound::loop_3D(*organ_filler_sample, 1.0f, target->position, 1.0f);
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

	//move camera:
	{

		//combine inputs into a move:
		if (!stop_move){
			constexpr float PlayerSpeed = 3.0f;
			glm::vec3 move = glm::vec3(0.0f);
			if (left.pressed && !right.pressed && player->position.x > -0.8f) move.x =-1.0f;
			if (!left.pressed && right.pressed && player->position.x < float(GRID_SIZE * UNIT_SIZE)-1.4f) move.x = 1.0f;
			if (down.pressed && !up.pressed && player->position.y > -0.8f) move.y =-1.0f;
			if (!down.pressed && up.pressed && player->position.y < float(GRID_SIZE * UNIT_SIZE)-1.4f) move.y = 1.0f;

			if (move != glm::vec3(0.0f)) {
				player->rotation = glm::quatLookAt(-glm::normalize(move), glm::vec3(0, 0, 1));
				move = glm::normalize(move) * PlayerSpeed * elapsed;
				player->position += move;
				camera->transform->position += move;
			}
		}
		
		check_player_pos(player->position.x, player->position.y);

		if (player_pos_2d == target_position) {
			if (path.size() == 0){
				ending_timer += elapsed;
				if(glm::length(player->position - target->position) < 0.6f)
					stop_move = true;
				play_victory_effect();
				if(height < 4.0f)
					height += elapsed * 5;
				if(energy < 15.0f)
					energy *= 1.1f;
				cutoff += elapsed / 4;
				if(color < 0.9f)
					color += elapsed / 4;
				if (ending_timer > 5){
					reset_game(false);
				}
			}else{
				score ++;
				target_position = path.front();
				path.pop_front();
				target->position = glm::vec3(target_position.x * UNIT_SIZE, target_position.y * UNIT_SIZE, 0);
				target_loop->set_position(target->position);
			}
		}
		
		if (grid[player_pos_2d.x][player_pos_2d.y] == 0){
			//went wrong direction
			wrong_timer += elapsed;
			if (wrong_timer > grace_time+1.5){
				//reset game
				reset_game(true);
			}else if (wrong_timer > grace_time){
				//falling down
				play_death_effect();
				float FallSpeed = 9.8f*(wrong_timer-grace_time);
				stop_move = true;
				wrong_cube = cube_vec[player_pos];
				wrong_cube->position += FallSpeed*glm::vec3(0.0f, 0.0f, -1.0f)*elapsed;
				player->position += FallSpeed*glm::vec3(0.0f, 0.0f, -1.0f)*elapsed;
			}
		}else{
			wrong_timer = 0.0f;
		}
	}
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

void PlayMode::reset_game(bool ending){
	cutoff = 0.18f;
	energy = 1.0f;
	color = 0.7f;
	height = 1.25f;
	randomize_grid();
	glm::ivec2 initial_player_pos = path.front();
	path.pop_front();
	player->position.x = (float)initial_player_pos.x * UNIT_SIZE;
	player->position.y = (float)initial_player_pos.y * UNIT_SIZE;
	player->position.z = 0.0f;
	check_player_pos(player->position.x, player->position.y);
	camera->transform->position.x = camera_origin_pos.x+initial_player_pos.x * UNIT_SIZE;
	camera->transform->position.y = camera_origin_pos.y+initial_player_pos.y * UNIT_SIZE;
	target_position = path.front();
	path.pop_front();
	target->position = glm::vec3(target_position.x * UNIT_SIZE, target_position.y * UNIT_SIZE, 0);
	target_loop = Sound::loop_3D(*organ_filler_sample, 1.0f, target->position, 1.0f);

	if(ending){
		wrong_cube->position.z = -1.0f;
		stop_move = false;
		wrong_timer = 0.0f;
		grace_time = 2.0f;
		score = 0;
	}else{
		if (grace_time > 0.5)
			grace_time -= 0.5;
		stop_move = false;
	}

}

void PlayMode::draw(glm::uvec2 const &drawable_size) {
	//update camera aspect ratio for drawable:
	camera->aspect = float(drawable_size.x) / float(drawable_size.y);

	//set up light type and position for lit_color_texture_program:
	// TODO: consider using the Light(s) in the scene to do this

	glUseProgram(lit_color_texture_program->program);
	glUniform1i(lit_color_texture_program->LIGHT_TYPE_int, 2);
	glUniform3fv(lit_color_texture_program->LIGHT_LOCATION_vec3, 1, glm::value_ptr(player->position + (glm::vec3(0.0f, 0.0f, height))));
	glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f, -1.0f)));
	glUniform1f(lit_color_texture_program->LIGHT_CUTOFF_float, std::cos(3.1415926f * cutoff));
	glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(energy * glm::vec3(1.0f, 1.0f, color)));
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
		lines.draw_text("WASD moves       Score: "+std::to_string(score),
			glm::vec3(-aspect + 0.1f * H, -1.0 + 0.1f * H, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0x00, 0x00, 0x00, 0x00));
		float ofs = 2.0f / drawable_size.y;
		lines.draw_text("WASD moves       Score: "+std::to_string(score),
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
void PlayMode::play_death_effect() {
	if (!target_loop->stopped) {
		target_loop->stop();
		Sound::play(*maybe_next_time_sample, 1.0f, 0.0f);
	}
}
void PlayMode::play_victory_effect() {
	if (!target_loop->stopped) {
		target_loop->stop();
		Sound::play(*nice_one_sample, 1.0f, 0.0f);
	}
}
