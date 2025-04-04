#ifndef USD_PLUGIN_H
#define USD_PLUGIN_H

#include <godot_cpp/classes/editor_plugin.hpp>
#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/classes/button.hpp>

namespace godot {

class USDPlugin : public EditorPlugin {
    GDCLASS(USDPlugin, EditorPlugin);

private:
    Button *hello_button;

protected:
    static void _bind_methods();

public:
    USDPlugin();
    ~USDPlugin();

    virtual void _enter_tree() override;
    virtual void _exit_tree() override;
    virtual bool _has_main_screen() const override;
    virtual String _get_plugin_name() const override;
    
    void _on_hello_button_pressed();
};

}

#endif // USD_PLUGIN_H
