/*
* Functions and classes to wrap around the
* various WebGL interfaces.
*/

let canvas = document.getElementById("sketch-canvas");
let context = "webgl2";
let gl = canvas.getContext(context);
let ext = null;
let ext2 = null;
if (gl === null) {
    console.log("WebGL2 not available.");
    context = "webgl";
    gl = canvas.getContext(context);
    if (gl === null) {
        throw "Your browser does not support WebGL."
    }
    ext = gl.getExtension('OES_texture_float');
    ext2 = gl.getExtension('OES_texture_float_linear');
    if (ext === null && ext2 === null) {
        throw "Your browser does not support the necessary WebGL extensions."
    }
} else {
    ext = gl.getExtension('EXT_color_buffer_float');
    if (ext === null) {
        throw "Your browser does not support the necessary WebGL extensions."
    }
}

if (gl === null) {
    document.getElementById("error-text").textContent = `<br>Unable to display canvas.`;
}

function makeShader(shaderType, shaderSource) {
    let shaderID = gl.createShader(shaderType);
    if (shaderID === 0) {
        alert("Unable to create shader.");
    }
    gl.shaderSource(shaderID, shaderSource);
    gl.compileShader(shaderID);
    if (!gl.getShaderParameter(shaderID, gl.COMPILE_STATUS)) {
        let msg = gl.getShaderInfoLog(shaderID);
        alert(`Unable to compile shader.\n${msg}`);
        gl.deleteShader(shaderID);
    }
    return shaderID;
}

function makeProgram(...shaderIDs) {
    let shaderProgram = gl.createProgram();
    for (let shaderID of shaderIDs) {
        gl.attachShader(shaderProgram, shaderID);
    }
    gl.linkProgram(shaderProgram);
    if (!gl.getProgramParameter(shaderProgram, gl.LINK_STATUS)) {
        alert(
            'Unable to initialize the shader program: '
            + gl.getProgramInfoLog(shaderProgram));
    }
    return shaderProgram;
}

function unbind() {
    gl.bindBuffer(gl.ARRAY_BUFFER, null);
    gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, null);
    gl.bindFramebuffer(gl.FRAMEBUFFER, null);
    gl.bindRenderbuffer(gl.RENDERBUFFER, null);
}

function makeTexture(buf, w, h, format) {
    let texture = gl.createTexture();
    gl.bindTexture(gl.TEXTURE_2D, texture);
    if (format === gl.FLOAT) {
        gl.texImage2D(
            gl.TEXTURE_2D, 0, 
            (context === "webgl")? gl.RGBA: gl.RGBA32F, 
            w, h, 0, 
            gl.RGBA, gl.FLOAT, 
            buf
        );
    } else {
        gl.texImage2D(
            gl.TEXTURE_2D, 0, 
            gl.RGBA, w, h, 0, 
            gl.RGBA, gl.UNSIGNED_BYTE, 
            buf
        );
    }
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST);
    if (context == "webgl") {
        // gl.generateMipmap(gl.TEXTURE_2D);
    } else {
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST);
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST);
    }
    return texture;
}

function makeImageTexture(image) {
    let texture = gl.createTexture();
    gl.bindTexture(gl.TEXTURE_2D, texture);
    gl.texImage2D(
        gl.TEXTURE_2D, 0, 
        gl.RGBA,
        gl.RGBA, gl.UNSIGNED_BYTE, 
        image
    );
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
    if (context == "webgl") {
        gl.generateMipmap(gl.TEXTURE_2D);
    } else {
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
    }
    return texture;
}

class Frame {
    constructor(w, h, frameNumber) {
        unbind();
        this.shaderProgram = null;
        this.uniforms = {};
        this.frameNumber = frameNumber;
        this.frameTexture = null;
        if (this.frameNumber !== 0) {
            gl.activeTexture(gl.TEXTURE0 + frameNumber);
            this.frameTexture = makeTexture(null, w, h, gl.FLOAT);
            gl.bindTexture(gl.TEXTURE_2D, this.frameTexture);
        }
        this.vbo = gl.createBuffer();
        this.ebo = gl.createBuffer();
        this.fbo = gl.createFramebuffer();
        if (this.frameNumber !== 0) {
            gl.bindFramebuffer(gl.FRAMEBUFFER, this.fbo);
            gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, 
                                    gl.TEXTURE_2D, this.frameTexture, 0);
        }
        unbind();
    }
    setFloatUniforms(uniforms) {
        for (let field of Object.keys(uniforms)) {
            this.uniforms[field] = gl.getUniformLocation(this.shaderProgram, field);
            gl.uniform1f(this.uniforms[field], uniforms[field]);
        }
    }
    setIntUniforms(uniforms) {
        for (let field of Object.keys(uniforms)) {
            this.uniforms[field] = gl.getUniformLocation(this.shaderProgram, field);
            gl.uniform1i(this.uniforms[field], uniforms[field]);
        }
    }
    setVec2Uniforms(uniforms) {
        for (let field of Object.keys(uniforms)) {
            this.uniforms[field] = gl.getUniformLocation(this.shaderProgram, field);
            gl.uniform2fv(this.uniforms[field], uniforms[field]);
        }
    }
    setVec4Uniforms(uniforms) {
        for (let field of Object.keys(uniforms)) {
            this.uniforms[field] = gl.getUniformLocation(this.shaderProgram, field);
            gl.uniform4fv(this.uniforms[field], uniforms[field]);
        }
    }
    useProgram(shaderProgram) {
        this.shaderProgram = shaderProgram;
        gl.useProgram(shaderProgram);
    }
    bind() {
        if (this.frameNumber !== 0) {
            gl.bindFramebuffer(gl.FRAMEBUFFER, this.fbo);
        }
        let shaderProgram = this.shaderProgram;
        let vertices = new Float32Array([1.0, 1.0, 0.0, 1.0, -1.0, 0.0, 
            -1.0, -1.0, 0.0, -1.0, 1.0, 0.0]);
        let elements = new Uint16Array([0, 2, 3, 0, 1, 2]);
        gl.bindBuffer(gl.ARRAY_BUFFER, this.vbo);
        gl.bufferData(gl.ARRAY_BUFFER, vertices, gl.STATIC_DRAW);
        gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, this.ebo);
        gl.bufferData(gl.ELEMENT_ARRAY_BUFFER, elements, gl.STATIC_DRAW);
        let pos = gl.getAttribLocation(shaderProgram, 'pos');
        gl.enableVertexAttribArray(pos);
        gl.vertexAttribPointer(pos, 3, gl.FLOAT, false, 3*4, 0);
    }
}


