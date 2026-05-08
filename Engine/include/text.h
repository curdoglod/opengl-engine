#ifndef TEXT_GL_H
#define TEXT_GL_H

#include "component.h"
#include <SDL.h>
#include <SDL_ttf.h>
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include "color.h"

enum class TextAlignment {
    LEFT,
    CENTER,
    RIGHT
};

class TextComponent : public Component
{
public:
    TextComponent(int fontSize,
           const std::string& text,
           const Color& color  = Color(255, 255, 255, 255),
           TextAlignment align = TextAlignment::LEFT);
    TextComponent(int fontSize,
           const std::string& text,
           TextAlignment align = TextAlignment::LEFT);
    ~TextComponent();

    void setText(const std::string& newText);
    void setColor(const Color& newColor);
    void setColor(Uint8 r, Uint8 g, Uint8 b, Uint8 a = 255) { setColor(Color(r, g, b, a)); }
    void setAlignment(TextAlignment newAlignment);

    virtual void Init() override;

    void Render();

    virtual TextComponent* Clone() const override {
        return new TextComponent(fontSize, text, color, alignment);
    }

private:
    bool createTextureFromSurface(SDL_Surface* surface);
    void updateTexture();     
    void initRenderData();    

   
private:
    int fontSize;
    std::string text;
    Color color;
    TextAlignment alignment;

    
    TTF_Font* font;
    std::vector<unsigned char> fontDataBuffer;

    
    GLuint textureID;
    int textWidth;
    int textHeight;

    
    GLuint VAO, VBO, EBO;
};

#endif // TEXT_GL_H
