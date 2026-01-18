#pragma once

/**
 * @brief Base interface for all editor panels
 */
class IEditorPanel {
public:
    virtual ~IEditorPanel() = default;
    
    virtual void Render() = 0;
    virtual const char* GetName() const = 0;
    
    bool IsOpen() const { return m_isOpen; }
    void SetOpen(bool open) { m_isOpen = open; }
    
protected:
    bool m_isOpen = true;
};

