#include "text.h"
#include "object.h"
#include "Renderer.h"
#include "ArchiveUnpacker.h"
#include "ResourceManager.h"
#include <GL/glew.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include "engine.h"
#include "Utils.h"

static const char *vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;

uniform mat4 model;
uniform mat4 projection;

out vec2 TexCoord;

void main()
{
    gl_Position = projection * model * vec4(aPos, 0.0, 1.0);
    TexCoord = aTexCoord;
}
)";

static const char *fragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;

in vec2 TexCoord;

uniform sampler2D textTexture;
uniform vec4 textColor;

void main()
{
    vec4 sampled = texture(textTexture, TexCoord);
    FragColor = sampled * textColor;
}
)";

TextComponent::TextComponent(int fontSize, const std::string &text, const Color &color, TextAlignment align)
    : fontSize(fontSize), text(text), color(color), alignment(align), font(nullptr), textureID(0), textWidth(0), textHeight(0), VAO(0), VBO(0), EBO(0)
{
}
TextComponent::TextComponent(int fontSize, const std::string &text, TextAlignment align)
    : fontSize(fontSize), text(text), color(Color(255, 255, 255, 255)), alignment(align), font(nullptr), textureID(0), textWidth(0), textHeight(0), VAO(0), VBO(0), EBO(0)
{
}
TextComponent::~TextComponent()
{
    if (textureID)
    {
        glDeleteTextures(1, &textureID);
    }
    if (VAO)
    {
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
        glDeleteBuffers(1, &EBO);
    }
    if (font)
    {
        TTF_CloseFont(font);
    }
}

void TextComponent::Init()
{
    fontDataBuffer = Engine::GetDefaultArchive()->GetFile("Roboto-Black.ttf");
    SDL_RWops *rw = SDL_RWFromConstMem(fontDataBuffer.data(), fontDataBuffer.size());
    if (!rw)
    {
        std::cerr << "Failed to create SDL_RWops for font" << std::endl;
        return;
    }
    font = TTF_OpenFontRW(rw, 1, fontSize);
    if (!font)
    {
        std::cerr << "Failed to open font: " << TTF_GetError() << std::endl;
        return;
    }

    initRenderData();
    updateTexture();
}

void TextComponent::initRenderData()
{
    float vertices[] = {
        0.0f, 1.0f, 0.0f, 1.0f,
        1.0f, 1.0f, 1.0f, 1.0f,
        1.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f
    };

    unsigned int indices[] = {
        0, 1, 2,
        2, 3, 0};

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

bool TextComponent::createTextureFromSurface(SDL_Surface *surface)
{
    if (!surface)
        return false;

    if (textureID)
    {
        glDeleteTextures(1, &textureID);
        textureID = 0;
    }

    textWidth = surface->w;
    textHeight = surface->h;

    GLenum format = GL_RGBA;
    int bytesPerPixel = surface->format->BytesPerPixel;
    if (bytesPerPixel == 4)
    {
        format = GL_RGBA;
    }
    else
    {
        format = GL_RGB;
    }

    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexImage2D(GL_TEXTURE_2D, 0, format, textWidth, textHeight, 0, format,
                 GL_UNSIGNED_BYTE, surface->pixels);

    glBindTexture(GL_TEXTURE_2D, 0);

    return true;
}

void TextComponent::updateTexture()
{
    if (!font)
        return;

    SDL_Color sdlColor = {color.r, color.g, color.b, color.a};
    SDL_Surface *surface = TTF_RenderText_Blended(font, text.c_str(), sdlColor);
    if (!surface)
    {
        std::cerr << "Failed to create text surface: " << TTF_GetError() << std::endl;
        return;
    }

    // Keep text upload format predictable for OpenGL.
    SDL_Surface *convSurface = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(surface);

    if (!convSurface)
    {
        std::cerr << "SDL_ConvertSurfaceFormat failed: " << SDL_GetError() << std::endl;
        return;
    }

    createTextureFromSurface(convSurface);

    SDL_FreeSurface(convSurface);
}

void TextComponent::setText(const std::string &newText)
{
    text = newText;
    updateTexture();
}

void TextComponent::setColor(const Color &newColor)
{
    color = newColor;
    updateTexture();
}

void TextComponent::setAlignment(TextAlignment newAlignment)
{
    alignment = newAlignment;
}



void TextComponent::Render()
{
    glDisable(GL_DEPTH_TEST);
    if (!textureID || !VAO)
        return;

    float angle = object->GetAngle().z;
    Vector2 pos = object->GetPosition();
    Vector2 size = object->GetSize();

    glm::mat4 model(1.0f);

    switch (alignment)
    {
    case TextAlignment::LEFT:
        break;
    case TextAlignment::CENTER:
        pos.x += (size.x * 0.5f) - (textWidth * 0.5f);
        break;
    case TextAlignment::RIGHT:
        pos.x += (size.x) - textWidth;
        break;
    }
    pos.y += (size.y * 0.5f) - (textHeight * 0.5f);

    model = glm::translate(model, glm::vec3(pos.x, pos.y, 0.0f));

    model = glm::translate(model, glm::vec3(textWidth * 0.5f, textHeight * 0.5f, 0.0f));
    model = glm::rotate(model, glm::radians(angle), glm::vec3(0, 0, 1));
    model = glm::translate(model, glm::vec3(-textWidth * 0.5f, -textHeight * 0.5f, 0.0f));

    model = glm::scale(model, glm::vec3(textWidth, textHeight, 1.0f));

    glm::mat4 projection = Renderer::Get().GetOrthoProjection();

    GLuint prog = ResourceManager::Get().GetOrCreateShader("text", vertexShaderSource, fragmentShaderSource);
    glUseProgram(prog);

    struct Uniforms {
        GLint model, projection, textColor, textTexture;
    };
    static std::unordered_map<GLuint, Uniforms> uniformCache;
    auto it = uniformCache.find(prog);
    if (it == uniformCache.end()) {
        Uniforms u;
        u.model = glGetUniformLocation(prog, "model");
        u.projection = glGetUniformLocation(prog, "projection");
        u.textColor = glGetUniformLocation(prog, "textColor");
        u.textTexture = glGetUniformLocation(prog, "textTexture");
        uniformCache[prog] = u;
        it = uniformCache.find(prog);
    }
    const Uniforms& u = it->second;

    glUniformMatrix4fv(u.model, 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(u.projection, 1, GL_FALSE, glm::value_ptr(projection));

    glUniform4f(u.textColor, color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glUniform1i(u.textTexture, 0);

    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
}
