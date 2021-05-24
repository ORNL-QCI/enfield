import sys
from pathlib import Path
sys.path.insert(1, str(Path.home()) + '/.xacc')
import xacc
import numpy as np
import qiskit

tolerance=1.e-5

hardware_length=5
hardware_height=hardware_length

def generate_heavy_hex_placement_mat(length,height):
    placement_mat=[]
    counts=-1
    locations=np.empty(length*height,dtype=int).reshape(length,height)
    reverse_map=np.empty(2*length*height,dtype=int).reshape(length*height,2)
    for y in range(height):    
        for x in range(length):
            counts+=1
            locations[x,y] = counts
            reverse_map[counts,0]=x
            reverse_map[counts,1]=y

    for h_qubit in range(counts+1):
        x=reverse_map[h_qubit,0]
        y=reverse_map[h_qubit,1]
        if (x%2==0): 
            if (y-1 > -1):
                placement_mat.append([h_qubit,locations[x,y-1]])
            if (y+1 < height):
                placement_mat.append([h_qubit,locations[x,y+1]])
            if ((x+2)%4 == 0):
                if (y%4==0):
                    placement_mat.append([h_qubit,locations[x-1,y]])
                elif ((y+2)%4==0 and x+1<length):
                    placement_mat.append([h_qubit,locations[x+1,y]])
            else:
                if (y%4==0 and x+1<length):
                    placement_mat.append([h_qubit,locations[x+1,y]])
                elif((y+2)%4==0 and x-1>-1):
                    placement_mat.append([h_qubit,locations[x-1,y]])
        else:
            if (x-1 > -1):
                placement_mat.append([h_qubit,locations[x-1,y]])
            if (x+1 < length):
                placement_mat.append([h_qubit,locations[x+1,y]])

    return placement_mat

# Specify connectivity, get QPP on that connectivity
connectivity = generate_heavy_hex_placement_mat(hardware_length,hardware_height)
#connectivity = [[0, 1], [1, 0], [1, 2], [2, 1], [3, 2], [2, 3],
#				[3, 4], [4, 3], [4, 5], [5, 4], [5, 0], [0, 5]]
#connectivity = [[0, 1], [1, 0], [1, 2], [2, 1], [3, 2], [2, 3]]
qpu = xacc.getAccelerator('qpp', {'connectivity':connectivity})
# Create StateVector calculator
state_vector_calc = xacc.getAccelerator("aer", {"sim-type": "statevector"})
# Create MaxCut Hamiltonian
H = xacc.getObservable('pauli', '')
edges=[[0,3],[0,4], [1,4], [2,4], [3,4]]
for i,j in edges:#, [3,0]]:
    H += 0.5 * xacc.getObservable('pauli', '1.0 I - Z{} Z{}'.format(i,j))


# Create QAOA Ansatz
qaoa_ansatz = xacc.createComposite('qaoa')
qaoa_ansatz.expand({'nbQubits':H.nBits(), 'nbSteps':1, 'cost-ham':H, 'parameter-scheme':'Standard'})
#print('Original QAOA Circ:\n',qaoa_ansatz.toString())
# Create random params
params = np.random.uniform(-2,2,2)
params[0]=1.0
params[1]=1.0
# Evaluate QAOA circuit at these params
evaled_qaoa_ansatz = qaoa_ansatz.eval(params)


# Apply the swap-shortest-path placement strategy
# print('Original Circuit:\n', evaled_qaoa_ansatz.toString())
evaled_qaoa_ansatz_openqasm = xacc.getCompiler('staq').translate(evaled_qaoa_ansatz)
circ_evaled = qiskit.QuantumCircuit.from_qasm_str(evaled_qaoa_ansatz_openqasm)
#circ_evaled.draw(output='mpl', filename='original.png')

# List of mappers:
# I listed in the order of *performance* (from my testing)
#   - 'Q_sabre': SABRE method (https://arxiv.org/abs/1809.02573)
#   - 'Q_chw': IBM Challenge Winner (https://arxiv.org/abs/1712.04722)
#   - 'Q_wpm': Weighted Partial Mapper
#   - 'Q_bmt': Bounded Mapping Tree
#   and many others that I didn't test.
# To use enfield:
placement_name = "enfield"
mapper = "Q_sabre"
placement = xacc.getIRTransformation(placement_name)
placement.apply(evaled_qaoa_ansatz, qpu, {"allocator": mapper})

evaled_routed_qaoa_ansatz_openqasm = xacc.getCompiler('staq').translate(evaled_qaoa_ansatz)
circ_evaled_placed = qiskit.QuantumCircuit.from_qasm_str(evaled_routed_qaoa_ansatz_openqasm)
circ_evaled_placed.draw(output='mpl', filename='test_placement.png')

