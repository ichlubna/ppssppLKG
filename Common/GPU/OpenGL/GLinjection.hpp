#include <iostream>
#include <map>
#include <stdexcept>
#include <vector>
#include <fstream>
#include <string>
#include <boost/algorithm/string/split.hpp> 
#include <boost/algorithm/string/classification.hpp> 

class GLInjection
{
    public:
    class HoloSettings
    {
        public: 
        std::map<std::string, float> params;

        HoloSettings(std::string fileName)
        {
            std::ifstream file(fileName);
            if(file.fail())
                throw std::runtime_error("Cannot open file: "+fileName);
            std::string line;
            while (std::getline(file, line))
            {
                std::vector<std::string> tokens;
                boost::split(tokens, line, boost::is_any_of("="), boost::token_compress_on);
                params[tokens[0]] = std::stof(tokens[1]);
            }
        }
        float operator[](std::string key){ return params[key];}
        int i(std::string key){ return static_cast<int>(params[key]);}
    };

    GLInjection(int renderWidth, int renderHeight, std::string configFile) : params{configFile}
    {    
        viewWidth = renderWidth;
        viewHeight = renderHeight;
        quiltWidth = viewWidth*params["Cols"];
        quiltHeight = viewHeight*params["Rows"]; 
        GLint drawFbo = 0, readFbo = 0;
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFbo);
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFbo);

        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glGenTextures(1, &fboTexture);
        glBindTexture(GL_TEXTURE_2D, fboTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, quiltWidth, quiltHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboTexture, 0);

        if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)  
            std::cerr << "Framebuffer creation failed" << std::endl;

        unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
        glCompileShader(vertexShader); 
        int success;
        char infoLog[512];
        glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
        if (!success)
        {
            glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
            std::cerr << "Holo vertex shader:" << std::endl;
            std::cerr << infoLog;
        }

        unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
        glCompileShader(fragmentShader);
        glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
        if (!success)
        {
            glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
            std::cerr << "Injected fragment shader:" << std::endl;
            std::cerr << infoLog;
        }

        shaderProgram = glCreateProgram();
        glAttachShader(shaderProgram, vertexShader);
        glAttachShader(shaderProgram, fragmentShader);
        glLinkProgram(shaderProgram);
        glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
            std::cerr << "Injected shader program:";
            std::cerr <<  infoLog;
        }
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        glGenVertexArrays(1, &emptyVAO);
        
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, drawFbo); 
        glBindFramebuffer(GL_READ_FRAMEBUFFER, drawFbo); 
    }

    void captureRender(int viewID)
    {
        GLint originalFbo = 0;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &originalFbo);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, originalFbo); 
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);
        int col = viewID % params.i("Cols");
        int row = viewID / params.i("Cols");
        glBlitFramebuffer(0, 0, viewWidth, viewHeight, col*viewWidth, row*viewHeight, (col+1)*viewWidth, (row+1)*viewHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        glBindFramebuffer(GL_FRAMEBUFFER, originalFbo);
    }

    void render(bool renderHolo=true)
    {
        GLint originalFbo = 0;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &originalFbo);

        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, originalFbo); 
        glBindTexture(GL_TEXTURE_2D, fboTexture);
        glUseProgram(shaderProgram);  
/*
        glUniform1f(glGetUniformLocation(shaderProgram, "holoTilt"), params["Tilt"]);
        glUniform1f(glGetUniformLocation(shaderProgram, "holoPitch"), params["Pitch"]);
        glUniform1f(glGetUniformLocation(shaderProgram, "holoCenter"), params["Center"]);
        glUniform1f(glGetUniformLocation(shaderProgram, "holoViewPortionElement"), params["ViewPortionElement"]);
        glUniform1f(glGetUniformLocation(shaderProgram, "holoSubp"), params["Subp"]);
        glUniform1i(glGetUniformLocation(shaderProgram, "viewCount"), views());
        glUniform1i(glGetUniformLocation(shaderProgram, "holoCols"), params["Cols"]);
        glUniform1i(glGetUniformLocation(shaderProgram, "holoRows"), params["Rows"]);*/
        glUniform1i(glGetUniformLocation(shaderProgram, "mode"), renderHolo);
        
        glBindVertexArray(emptyVAO);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        
        glBindFramebuffer(GL_FRAMEBUFFER, originalFbo);
    }

    int views()
    {
        return params.i("Rows")*params.i("Cols");
    } 

    float viewOffset(int viewID, float distance)
    {
        return (viewID - ((views()-1)/2.0f))*distance;
    }

    float cameraStep()
    {
        return params["CameraSpacingStep"];
    }
    
    float focusStep()
    {
        return params["FocusSpacingStep"];
    }
    
    private:
    int viewWidth;
    int viewHeight;
    HoloSettings params;
    GLuint fbo;
    GLuint fboTexture;
    GLuint shaderProgram;
    GLuint emptyVAO;
    int quiltWidth;
    int quiltHeight;
    const char *vertexShaderSource = R""""(
        #version 330 core
        out vec2 uv;

        void main()
        {
            vec2 triangle[3] = vec2[3](vec2(-1,-1), vec2(3,-1), vec2(-1, 3));
            gl_Position = vec4(triangle[gl_VertexID], 0, 1);
            uv = 0.5f * gl_Position.xy + vec2(0.5f);
        }
        )"""";
    const char *fragmentShaderSource =  R""""(
        #version 330 core

        uniform sampler2D fboTexture;
        uniform float holoTilt;
        uniform float holoPitch;
        uniform float holoCenter;
        uniform float holoViewPortionElement;
        uniform float holoSubp;
        uniform int viewCount;
        uniform int holoCols;
        uniform int holoRows;
        uniform int mode;
        in vec2 uv;
        out vec4 fragColor;

        const int MODE_HOLO = 0;
        const int MODE_QUILT = 1;

        vec2 texArr(vec3 uvz)
        {
            int viewsCount = holoCols * holoRows;
            float z = floor(uvz.z * viewsCount);
            float x = (mod(z, holoCols) + uvz.x) / holoCols;
            float y = (floor(z / holoCols) + uvz.y) / holoRows;
            return vec2(x, y) * vec2(holoViewPortionElement, holoViewPortionElement);
        }

        void createHoloView()
        {
            float invView = 1.0f;
            int ri = 0;
            int bi = 2;

            vec2 texCoords = uv;
            vec3 nuv = vec3(texCoords.xy, 0.0);

            vec4 rgb[3];
            for (int i=0; i < 3; i++) 
            {
                nuv.z = (texCoords.x + i * holoSubp + texCoords.y * holoTilt) * holoPitch - holoCenter;
                nuv.z = mod(nuv.z + ceil(abs(nuv.z)), 1.0);
                nuv.z = (1.0 - invView) * nuv.z + invView * (1.0 - nuv.z);
                vec2 coords = texArr(nuv);
                rgb[i] = texture2D(fboTexture, coords);
            }

            vec4 color = vec4(rgb[ri].r, rgb[1].g, rgb[bi].b, 1.0);
            fragColor = color;
        }

        void main(void)
        {
            if(mode == MODE_QUILT)
                fragColor = texture(fboTexture, uv);
            else if(mode == MODE_HOLO)
                createHoloView();
        }
        )"""";

};
