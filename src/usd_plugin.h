#ifndef USD_PLUGIN_H
#define USD_PLUGIN_H

#include <godot_cpp/classes/editor_plugin.hpp>
#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/variant/string.hpp>

namespace godot {

class USDPlugin : public EditorPlugin {
    GDCLASS(USDPlugin, EditorPlugin);

protected:
    static void _bind_methods();

public:
    USDPlugin();
    ~USDPlugin();

    virtual void _enter_tree() override;
    virtual void _exit_tree() override;
    virtual bool _has_main_screen() const override;
    virtual String _get_plugin_name() const override;
};

}

#endif // USD_PLUGIN_H
