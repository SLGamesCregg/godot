/*************************************************************************/
/*  input_map.cpp                                                        */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2021 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2021 Godot Engine contributors (cf. AUTHORS.md).   */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#include "input_map.h"

#include "core/config/project_settings.h"
#include "core/input/input.h"
#include "core/os/keyboard.h"

InputMap *InputMap::singleton = nullptr;

int InputMap::ALL_DEVICES = -1;

void InputMap::_bind_methods() {
	ClassDB::bind_method(D_METHOD("has_action", "action"), &InputMap::has_action);
	ClassDB::bind_method(D_METHOD("get_actions"), &InputMap::_get_actions);
	ClassDB::bind_method(D_METHOD("add_action", "action", "deadzone"), &InputMap::add_action, DEFVAL(0.5f));
	ClassDB::bind_method(D_METHOD("erase_action", "action"), &InputMap::erase_action);

	ClassDB::bind_method(D_METHOD("action_set_deadzone", "action", "deadzone"), &InputMap::action_set_deadzone);
	ClassDB::bind_method(D_METHOD("action_get_deadzone", "action"), &InputMap::action_get_deadzone);
	ClassDB::bind_method(D_METHOD("action_add_event", "action", "event"), &InputMap::action_add_event);
	ClassDB::bind_method(D_METHOD("action_has_event", "action", "event"), &InputMap::action_has_event);
	ClassDB::bind_method(D_METHOD("action_erase_event", "action", "event"), &InputMap::action_erase_event);
	ClassDB::bind_method(D_METHOD("action_erase_events", "action"), &InputMap::action_erase_events);
	ClassDB::bind_method(D_METHOD("action_get_events", "action"), &InputMap::_action_get_events);
	ClassDB::bind_method(D_METHOD("event_is_action", "event", "action", "exact_match"), &InputMap::event_is_action, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("load_from_project_settings"), &InputMap::load_from_project_settings);
}

/**
 * Returns an nonexistent action error message with a suggestion of the closest
 * matching action name (if possible).
 */
String InputMap::_suggest_actions(const StringName &p_action) const {
	List<StringName> actions = get_actions();
	StringName closest_action;
	float closest_similarity = 0.0;

	// Find the most action with the most similar name.
	for (StringName &action : actions) {
		const float similarity = String(action).similarity(p_action);

		if (similarity > closest_similarity) {
			closest_action = action;
			closest_similarity = similarity;
		}
	}

	String error_message = vformat("The InputMap action \"%s\" doesn't exist.", p_action);

	if (closest_similarity >= 0.4) {
		// Only include a suggestion in the error message if it's similar enough.
		error_message += vformat(" Did you mean \"%s\"?", closest_action);
	}
	return error_message;
}

void InputMap::add_action(const StringName &p_action, float p_deadzone) {
	ERR_FAIL_COND_MSG(input_map.has(p_action), "InputMap already has action \"" + String(p_action) + "\".");
	input_map[p_action] = Action();
	static int last_id = 1;
	input_map[p_action].id = last_id;
	input_map[p_action].deadzone = p_deadzone;
	last_id++;
}

void InputMap::erase_action(const StringName &p_action) {
	ERR_FAIL_COND_MSG(!input_map.has(p_action), _suggest_actions(p_action));

	input_map.erase(p_action);
}

Array InputMap::_get_actions() {
	Array ret;
	List<StringName> actions = get_actions();
	if (actions.is_empty()) {
		return ret;
	}

	for (const StringName &E : actions) {
		ret.push_back(E);
	}

	return ret;
}

List<StringName> InputMap::get_actions() const {
	List<StringName> actions = List<StringName>();
	if (input_map.is_empty()) {
		return actions;
	}

	for (OrderedHashMap<StringName, Action>::Element E = input_map.front(); E; E = E.next()) {
		actions.push_back(E.key());
	}

	return actions;
}

List<Ref<InputEvent>>::Element *InputMap::_find_event(Action &p_action, const Ref<InputEvent> &p_event, bool p_exact_match, bool *p_pressed, float *p_strength, float *p_raw_strength) const {
	ERR_FAIL_COND_V(!p_event.is_valid(), nullptr);

	for (List<Ref<InputEvent>>::Element *E = p_action.inputs.front(); E; E = E->next()) {
		int device = E->get()->get_device();
		if (device == ALL_DEVICES || device == p_event->get_device()) {
			if (p_exact_match && E->get()->is_match(p_event, true)) {
				return E;
			} else if (!p_exact_match && E->get()->action_match(p_event, p_pressed, p_strength, p_raw_strength, p_action.deadzone)) {
				return E;
			}
		}
	}

	return nullptr;
}

bool InputMap::has_action(const StringName &p_action) const {
	return input_map.has(p_action);
}

float InputMap::action_get_deadzone(const StringName &p_action) {
	ERR_FAIL_COND_V_MSG(!input_map.has(p_action), 0.0f, _suggest_actions(p_action));

	return input_map[p_action].deadzone;
}

void InputMap::action_set_deadzone(const StringName &p_action, float p_deadzone) {
	ERR_FAIL_COND_MSG(!input_map.has(p_action), _suggest_actions(p_action));

	input_map[p_action].deadzone = p_deadzone;
}

void InputMap::action_add_event(const StringName &p_action, const Ref<InputEvent> &p_event) {
	ERR_FAIL_COND_MSG(p_event.is_null(), "It's not a reference to a valid InputEvent object.");
	ERR_FAIL_COND_MSG(!input_map.has(p_action), _suggest_actions(p_action));
	if (_find_event(input_map[p_action], p_event, true)) {
		return; // Already added.
	}

	input_map[p_action].inputs.push_back(p_event);
}

bool InputMap::action_has_event(const StringName &p_action, const Ref<InputEvent> &p_event) {
	ERR_FAIL_COND_V_MSG(!input_map.has(p_action), false, _suggest_actions(p_action));
	return (_find_event(input_map[p_action], p_event, true) != nullptr);
}

void InputMap::action_erase_event(const StringName &p_action, const Ref<InputEvent> &p_event) {
	ERR_FAIL_COND_MSG(!input_map.has(p_action), _suggest_actions(p_action));

	List<Ref<InputEvent>>::Element *E = _find_event(input_map[p_action], p_event, true);
	if (E) {
		input_map[p_action].inputs.erase(E);
		if (Input::get_singleton()->is_action_pressed(p_action)) {
			Input::get_singleton()->action_release(p_action);
		}
	}
}

void InputMap::action_erase_events(const StringName &p_action) {
	ERR_FAIL_COND_MSG(!input_map.has(p_action), _suggest_actions(p_action));

	input_map[p_action].inputs.clear();
}

Array InputMap::_action_get_events(const StringName &p_action) {
	Array ret;
	const List<Ref<InputEvent>> *al = action_get_events(p_action);
	if (al) {
		for (const List<Ref<InputEvent>>::Element *E = al->front(); E; E = E->next()) {
			ret.push_back(E);
		}
	}

	return ret;
}

const List<Ref<InputEvent>> *InputMap::action_get_events(const StringName &p_action) {
	const OrderedHashMap<StringName, Action>::Element E = input_map.find(p_action);
	if (!E) {
		return nullptr;
	}

	return &E.get().inputs;
}

bool InputMap::event_is_action(const Ref<InputEvent> &p_event, const StringName &p_action, bool p_exact_match) const {
	return event_get_action_status(p_event, p_action, p_exact_match);
}

bool InputMap::event_get_action_status(const Ref<InputEvent> &p_event, const StringName &p_action, bool p_exact_match, bool *p_pressed, float *p_strength, float *p_raw_strength) const {
	OrderedHashMap<StringName, Action>::Element E = input_map.find(p_action);
	ERR_FAIL_COND_V_MSG(!E, false, _suggest_actions(p_action));

	Ref<InputEventAction> input_event_action = p_event;
	if (input_event_action.is_valid()) {
		if (p_pressed != nullptr) {
			*p_pressed = input_event_action->is_pressed();
		}
		if (p_strength != nullptr) {
			*p_strength = (p_pressed != nullptr && *p_pressed) ? input_event_action->get_strength() : 0.0f;
		}
		return input_event_action->get_action() == p_action;
	}

	bool pressed;
	float strength;
	float raw_strength;
	List<Ref<InputEvent>>::Element *event = _find_event(E.get(), p_event, p_exact_match, &pressed, &strength, &raw_strength);
	if (event != nullptr) {
		if (p_pressed != nullptr) {
			*p_pressed = pressed;
		}
		if (p_strength != nullptr) {
			*p_strength = strength;
		}
		if (p_raw_strength != nullptr) {
			*p_raw_strength = raw_strength;
		}
		return true;
	} else {
		return false;
	}
}

const OrderedHashMap<StringName, InputMap::Action> &InputMap::get_action_map() const {
	return input_map;
}

void InputMap::load_from_project_settings() {
	input_map.clear();

	List<PropertyInfo> pinfo;
	ProjectSettings::get_singleton()->get_property_list(&pinfo);

	for (PropertyInfo &pi : pinfo) {
		if (!pi.name.begins_with("input/")) {
			continue;
		}

		String name = pi.name.substr(pi.name.find("/") + 1, pi.name.length());

		Dictionary action = ProjectSettings::get_singleton()->get(pi.name);
		float deadzone = action.has("deadzone") ? (float)action["deadzone"] : 0.5f;
		Array events = action["events"];

		add_action(name, deadzone);
		for (int i = 0; i < events.size(); i++) {
			Ref<InputEvent> event = events[i];
			if (event.is_null()) {
				continue;
			}
			action_add_event(name, event);
		}
	}
}

struct _BuiltinActionDisplayName {
	const char *name;
	const char *display_name;
};

static const _BuiltinActionDisplayName _builtin_action_display_names[] = {
	/* clang-format off */
    { "ui_accept",                                     TTRC("Accept") },
    { "ui_select",                                     TTRC("Select") },
    { "ui_cancel",                                     TTRC("Cancel") },
    { "ui_focus_next",                                 TTRC("Focus Next") },
    { "ui_focus_prev",                                 TTRC("Focus Prev") },
    { "ui_left",                                       TTRC("Left") },
    { "ui_right",                                      TTRC("Right") },
    { "ui_up",                                         TTRC("Up") },
    { "ui_down",                                       TTRC("Down") },
    { "ui_page_up",                                    TTRC("Page Up") },
    { "ui_page_down",                                  TTRC("Page Down") },
    { "ui_home",                                       TTRC("Home") },
    { "ui_end",                                        TTRC("End") },
    { "ui_cut",                                        TTRC("Cut") },
    { "ui_copy",                                       TTRC("Copy") },
    { "ui_paste",                                      TTRC("Paste") },
    { "ui_undo",                                       TTRC("Undo") },
    { "ui_redo",                                       TTRC("Redo") },
    { "ui_text_completion_query",                      TTRC("Completion Query") },
    { "ui_text_newline",                               TTRC("New Line") },
    { "ui_text_newline_blank",                         TTRC("New Blank Line") },
    { "ui_text_newline_above",                         TTRC("New Line Above") },
    { "ui_text_indent",                                TTRC("Indent") },
    { "ui_text_dedent",                                TTRC("Dedent") },
    { "ui_text_backspace",                             TTRC("Backspace") },
    { "ui_text_backspace_word",                        TTRC("Backspace Word") },
    { "ui_text_backspace_word.OSX",                    TTRC("Backspace Word") },
    { "ui_text_backspace_all_to_left",                 TTRC("Backspace all to Left") },
    { "ui_text_backspace_all_to_left.OSX",             TTRC("Backspace all to Left") },
    { "ui_text_delete",                                TTRC("Delete") },
    { "ui_text_delete_word",                           TTRC("Delete Word") },
    { "ui_text_delete_word.OSX",                       TTRC("Delete Word") },
    { "ui_text_delete_all_to_right",                   TTRC("Delete all to Right") },
    { "ui_text_delete_all_to_right.OSX",               TTRC("Delete all to Right") },
    { "ui_text_caret_left",                            TTRC("Caret Left") },
    { "ui_text_caret_word_left",                       TTRC("Caret Word Left") },
    { "ui_text_caret_word_left.OSX",                   TTRC("Caret Word Left") },
    { "ui_text_caret_right",                           TTRC("Caret Right") },
    { "ui_text_caret_word_right",                      TTRC("Caret Word Right") },
    { "ui_text_caret_word_right.OSX",                  TTRC("Caret Word Right") },
    { "ui_text_caret_up",                              TTRC("Caret Up") },
    { "ui_text_caret_down",                            TTRC("Caret Down") },
    { "ui_text_caret_line_start",                      TTRC("Caret Line Start") },
    { "ui_text_caret_line_start.OSX",                  TTRC("Caret Line Start") },
    { "ui_text_caret_line_end",                        TTRC("Caret Line End") },
    { "ui_text_caret_line_end.OSX",                    TTRC("Caret Line End") },
    { "ui_text_caret_page_up",                         TTRC("Caret Page Up") },
    { "ui_text_caret_page_down",                       TTRC("Caret Page Down") },
    { "ui_text_caret_document_start",                  TTRC("Caret Document Start") },
    { "ui_text_caret_document_start.OSX",              TTRC("Caret Document Start") },
    { "ui_text_caret_document_end",                    TTRC("Caret Document End") },
    { "ui_text_caret_document_end.OSX",                TTRC("Caret Document End") },
    { "ui_text_scroll_up",                             TTRC("Scroll Up") },
    { "ui_text_scroll_up.OSX",                         TTRC("Scroll Up") },
    { "ui_text_scroll_down",                           TTRC("Scroll Down") },
    { "ui_text_scroll_down.OSX",                       TTRC("Scroll Down") },
    { "ui_text_select_all",                            TTRC("Select All") },
    { "ui_text_select_word_under_caret",               TTRC("Select Word Under Caret") },
    { "ui_text_toggle_insert_mode",                    TTRC("Toggle Insert Mode") },
    { "ui_text_submit",                                TTRC("Text Submitted") },
    { "ui_graph_duplicate",                            TTRC("Duplicate Nodes") },
    { "ui_graph_delete",                               TTRC("Delete Nodes") },
    { "ui_filedialog_up_one_level",                    TTRC("Go Up One Level") },
    { "ui_filedialog_refresh",                         TTRC("Refresh") },
    { "ui_filedialog_show_hidden",                     TTRC("Show Hidden") },
    { "ui_swap_input_direction ",                      TTRC("Swap Input Direction") },
    { "",                                              TTRC("")}
	/* clang-format on */
};

String InputMap::get_builtin_display_name(const String &p_name) const {
	int len = sizeof(_builtin_action_display_names) / sizeof(_BuiltinActionDisplayName);

	for (int i = 0; i < len; i++) {
		if (_builtin_action_display_names[i].name == p_name) {
			return RTR(_builtin_action_display_names[i].display_name);
		}
	}

	return p_name;
}

const OrderedHashMap<String, List<Ref<InputEvent>>> &InputMap::get_builtins() {
	// Return cache if it has already been built.
	if (default_builtin_cache.size()) {
		return default_builtin_cache;
	}

	List<Ref<InputEvent>> inputs;
	inputs.push_back(InputEventKey::create_reference(KEY_ENTER));
	inputs.push_back(InputEventKey::create_reference(KEY_KP_ENTER));
	inputs.push_back(InputEventKey::create_reference(KEY_SPACE));
	default_builtin_cache.insert("ui_accept", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventJoypadButton::create_reference(JOY_BUTTON_Y));
	inputs.push_back(InputEventKey::create_reference(KEY_SPACE));
	default_builtin_cache.insert("ui_select", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(KEY_ESCAPE));
	default_builtin_cache.insert("ui_cancel", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(KEY_TAB));
	default_builtin_cache.insert("ui_focus_next", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(KEY_TAB | KEY_MASK_SHIFT));
	default_builtin_cache.insert("ui_focus_prev", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(KEY_LEFT));
	inputs.push_back(InputEventJoypadButton::create_reference(JOY_BUTTON_DPAD_LEFT));
	default_builtin_cache.insert("ui_left", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(KEY_RIGHT));
	inputs.push_back(InputEventJoypadButton::create_reference(JOY_BUTTON_DPAD_RIGHT));
	default_builtin_cache.insert("ui_right", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(KEY_UP));
	inputs.push_back(InputEventJoypadButton::create_reference(JOY_BUTTON_DPAD_UP));
	default_builtin_cache.insert("ui_up", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(KEY_DOWN));
	inputs.push_back(InputEventJoypadButton::create_reference(JOY_BUTTON_DPAD_DOWN));
	default_builtin_cache.insert("ui_down", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(KEY_PAGEUP));
	default_builtin_cache.insert("ui_page_up", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(KEY_PAGEDOWN));
	default_builtin_cache.insert("ui_page_down", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(KEY_HOME));
	default_builtin_cache.insert("ui_home", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(KEY_END));
	default_builtin_cache.insert("ui_end", inputs);

	// ///// UI basic Shortcuts /////

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(KEY_X | KEY_MASK_CMD));
	inputs.push_back(InputEventKey::create_reference(KEY_DELETE | KEY_MASK_SHIFT));
	default_builtin_cache.insert("ui_cut", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(KEY_C | KEY_MASK_CMD));
	inputs.push_back(InputEventKey::create_reference(KEY_INSERT | KEY_MASK_CMD));
	default_builtin_cache.insert("ui_copy", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(KEY_V | KEY_MASK_CMD));
	inputs.push_back(InputEventKey::create_reference(KEY_INSERT | KEY_MASK_SHIFT));
	default_builtin_cache.insert("ui_paste", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(KEY_Z | KEY_MASK_CMD));
	default_builtin_cache.insert("ui_undo", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(KEY_Z | KEY_MASK_CMD | KEY_MASK_SHIFT));
	inputs.push_back(InputEventKey::create_reference(KEY_Y | KEY_MASK_CMD));
	default_builtin_cache.insert("ui_redo", inputs);

	// ///// UI Text Input Shortcuts /////
	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(KEY_SPACE | KEY_MASK_CMD));
	default_builtin_cache.insert("ui_text_completion_query", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(KEY_ENTER));
	inputs.push_back(InputEventKey::create_reference(KEY_KP_ENTER));
	default_builtin_cache.insert("ui_text_completion_accept", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(KEY_TAB));
	default_builtin_cache.insert("ui_text_completion_replace", inputs);

	// Newlines
	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(KEY_ENTER));
	inputs.push_back(InputEventKey::create_reference(KEY_KP_ENTER));
	default_builtin_cache.insert("ui_text_newline", inputs);

	inputs = List<Ref<InputEvent>>();

	inputs.push_back(InputEventKey::create_reference(KEY_ENTER | KEY_MASK_CMD));
	inputs.push_back(InputEventKey::create_reference(KEY_KP_ENTER | KEY_MASK_CMD));
	default_builtin_cache.insert("ui_text_newline_blank", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(KEY_ENTER | KEY_MASK_SHIFT | KEY_MASK_CMD));
	inputs.push_back(InputEventKey::create_reference(KEY_KP_ENTER | KEY_MASK_SHIFT | KEY_MASK_CMD));
	default_builtin_cache.insert("ui_text_newline_above", inputs);

	// Indentation
	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(KEY_TAB));
	default_builtin_cache.insert("ui_text_indent", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(KEY_TAB | KEY_MASK_SHIFT));
	default_builtin_cache.insert("ui_text_dedent", inputs);

	// Text Backspace and Delete
	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(KEY_BACKSPACE));
	inputs.push_back(InputEventKey::create_reference(KEY_BACKSPACE | KEY_MASK_SHIFT));
	default_builtin_cache.insert("ui_text_backspace", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(KEY_BACKSPACE | KEY_MASK_CMD));
	default_builtin_cache.insert("ui_text_backspace_word", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(KEY_BACKSPACE | KEY_MASK_ALT));
	default_builtin_cache.insert("ui_text_backspace_word.OSX", inputs);

	inputs = List<Ref<InputEvent>>();
	default_builtin_cache.insert("ui_text_backspace_all_to_left", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(KEY_BACKSPACE | KEY_MASK_CMD));
	default_builtin_cache.insert("ui_text_backspace_all_to_left.OSX", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(KEY_DELETE));
	default_builtin_cache.insert("ui_text_delete", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(KEY_DELETE | KEY_MASK_CMD));
	default_builtin_cache.insert("ui_text_delete_word", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(KEY_DELETE | KEY_MASK_ALT));
	default_builtin_cache.insert("ui_text_delete_word.OSX", inputs);

	inputs = List<Ref<InputEvent>>();
	default_builtin_cache.insert("ui_text_delete_all_to_right", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(KEY_DELETE | KEY_MASK_CMD));
	default_builtin_cache.insert("ui_text_delete_all_to_right.OSX", inputs);

	// Text Caret Movement Left/Right

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(KEY_LEFT));
	default_builtin_cache.insert("ui_text_caret_left", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(KEY_LEFT | KEY_MASK_CMD));
	default_builtin_cache.insert("ui_text_caret_word_left", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(KEY_LEFT | KEY_MASK_ALT));
	default_builtin_cache.insert("ui_text_caret_word_left.OSX", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(KEY_RIGHT));
	default_builtin_cache.insert("ui_text_caret_right", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(KEY_RIGHT | KEY_MASK_CMD));
	default_builtin_cache.insert("ui_text_caret_word_right", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(KEY_RIGHT | KEY_MASK_ALT));
	default_builtin_cache.insert("ui_text_caret_word_right.OSX", inputs);

	// Text Caret Movement Up/Down

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(KEY_UP));
	default_builtin_cache.insert("ui_text_caret_up", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(KEY_DOWN));
	default_builtin_cache.insert("ui_text_caret_down", inputs);

	// Text Caret Movement Line Start/End

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(KEY_HOME));
	default_builtin_cache.insert("ui_text_caret_line_start", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(KEY_A | KEY_MASK_CTRL));
	inputs.push_back(InputEventKey::create_reference(KEY_LEFT | KEY_MASK_CMD));
	default_builtin_cache.insert("ui_text_caret_line_start.OSX", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(KEY_END));
	default_builtin_cache.insert("ui_text_caret_line_end", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(KEY_E | KEY_MASK_CTRL));
	inputs.push_back(InputEventKey::create_reference(KEY_RIGHT | KEY_MASK_CMD));
	default_builtin_cache.insert("ui_text_caret_line_end.OSX", inputs);

	// Text Caret Movement Page Up/Down

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(KEY_PAGEUP));
	default_builtin_cache.insert("ui_text_caret_page_up", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(KEY_PAGEDOWN));
	default_builtin_cache.insert("ui_text_caret_page_down", inputs);

	// Text Caret Movement Document Start/End

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(KEY_HOME | KEY_MASK_CMD));
	default_builtin_cache.insert("ui_text_caret_document_start", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(KEY_UP | KEY_MASK_CMD));
	default_builtin_cache.insert("ui_text_caret_document_start.OSX", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(KEY_END | KEY_MASK_CMD));
	default_builtin_cache.insert("ui_text_caret_document_end", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(KEY_DOWN | KEY_MASK_CMD));
	default_builtin_cache.insert("ui_text_caret_document_end.OSX", inputs);

	// Text Scrolling

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(KEY_UP | KEY_MASK_CMD));
	default_builtin_cache.insert("ui_text_scroll_up", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(KEY_UP | KEY_MASK_CMD | KEY_MASK_ALT));
	default_builtin_cache.insert("ui_text_scroll_up.OSX", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(KEY_DOWN | KEY_MASK_CMD));
	default_builtin_cache.insert("ui_text_scroll_down", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(KEY_DOWN | KEY_MASK_CMD | KEY_MASK_ALT));
	default_builtin_cache.insert("ui_text_scroll_down.OSX", inputs);

	// Text Misc

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(KEY_A | KEY_MASK_CMD));
	default_builtin_cache.insert("ui_text_select_all", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(KEY_D | KEY_MASK_CMD));
	default_builtin_cache.insert("ui_text_select_word_under_caret", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(KEY_INSERT));
	default_builtin_cache.insert("ui_text_toggle_insert_mode", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(KEY_MENU));
	default_builtin_cache.insert("ui_menu", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(KEY_ENTER));
	inputs.push_back(InputEventKey::create_reference(KEY_KP_ENTER));
	default_builtin_cache.insert("ui_text_submit", inputs);

	// ///// UI Graph Shortcuts /////

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(KEY_D | KEY_MASK_CMD));
	default_builtin_cache.insert("ui_graph_duplicate", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(KEY_DELETE));
	default_builtin_cache.insert("ui_graph_delete", inputs);

	// ///// UI File Dialog Shortcuts /////
	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(KEY_BACKSPACE));
	default_builtin_cache.insert("ui_filedialog_up_one_level", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(KEY_F5));
	default_builtin_cache.insert("ui_filedialog_refresh", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(KEY_H));
	default_builtin_cache.insert("ui_filedialog_show_hidden", inputs);

	inputs = List<Ref<InputEvent>>();
	inputs.push_back(InputEventKey::create_reference(KEY_QUOTELEFT | KEY_MASK_CMD));
	default_builtin_cache.insert("ui_swap_input_direction", inputs);

	return default_builtin_cache;
}

void InputMap::load_default() {
	OrderedHashMap<String, List<Ref<InputEvent>>> builtins = get_builtins();

	// List of Builtins which have an override for OSX.
	Vector<String> osx_builtins;
	for (OrderedHashMap<String, List<Ref<InputEvent>>>::Element E = builtins.front(); E; E = E.next()) {
		if (String(E.key()).ends_with(".OSX")) {
			// Strip .OSX from name: some_input_name.OSX -> some_input_name
			osx_builtins.push_back(String(E.key()).split(".")[0]);
		}
	}

	for (OrderedHashMap<String, List<Ref<InputEvent>>>::Element E = builtins.front(); E; E = E.next()) {
		String fullname = E.key();
		String name = fullname.split(".")[0];
		String override_for = fullname.split(".").size() > 1 ? fullname.split(".")[1] : "";

#ifdef APPLE_STYLE_KEYS
		if (osx_builtins.has(name) && override_for != "OSX") {
			// Name has osx builtin but this particular one is for non-osx systems - so skip.
			continue;
		}
#else
		if (override_for == "OSX") {
			// Override for OSX - not needed on non-osx platforms.
			continue;
		}
#endif

		add_action(name);

		List<Ref<InputEvent>> inputs = E.get();
		for (List<Ref<InputEvent>>::Element *I = inputs.front(); I; I = I->next()) {
			Ref<InputEventKey> iek = I->get();

			// For the editor, only add keyboard actions.
			if (iek.is_valid()) {
				action_add_event(name, I->get());
			}
		}
	}
}

InputMap::InputMap() {
	ERR_FAIL_COND_MSG(singleton, "Singleton in InputMap already exist.");
	singleton = this;
}
