function createCustomBorder(orientation) {
    const border = document.createElement('div');
    border.className = `custom-${orientation}`;
  
    const middle = document.createElement('div');
    middle.className = `custom-${orientation}-middle`;
  
    const dot1 = document.createElement('div');
    dot1.className = `custom-${orientation}-dot`;
  
    const dot2 = document.createElement('div');
    dot2.className = `custom-${orientation}-dot`;
  
    middle.appendChild(dot1);
    middle.appendChild(dot2);
  
    const line = document.createElement('div');
    line.className = `custom-${orientation}-line`;
  
    border.appendChild(middle);
    border.appendChild(line);
    border.appendChild(middle.cloneNode(true));
    border.appendChild(line.cloneNode(true));
    border.appendChild(middle.cloneNode(true));
  
    return border;
  }

function addCustomBordersToAllBlocks() {
    const blocks = document.querySelectorAll('.block');
  
    blocks.forEach((block) => {
      const borderTop = document.createElement('div');
      borderTop.className = 'border-top';
      borderTop.appendChild(createCustomBorder('hr'));
      block.appendChild(borderTop);
  
      const borderBottom = document.createElement('div');
      borderBottom.className = 'border-bottom';
      borderBottom.appendChild(createCustomBorder('hr'));
      block.appendChild(borderBottom);
  
      const borderLeft = document.createElement('div');
      borderLeft.className = 'border-left';
      borderLeft.appendChild(createCustomBorder('vr'));
      block.appendChild(borderLeft);
  
      const borderRight = document.createElement('div');
      borderRight.className = 'border-right';
      borderRight.appendChild(createCustomBorder('vr'));
      block.appendChild(borderRight);
    });
}

document.addEventListener('DOMContentLoaded', () => {
    addCustomBordersToAllBlocks();
});
